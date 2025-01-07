#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <signal.h>
#include <pthread.h>

#include "common.h"
#include "protocol.h"

#define MAX_EVENTS 100

Agent agents[MAX_AGENTS];
int agents_iterator = 0;
Task task_queue[MAX_TASKS];
int task_count = 0;
Client clients[MAX_CLIENTS];
int clients_iterator = 0;
TaskResult result_queue[MAX_TASKS];
int result_count = 0;

pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agents_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct sockaddr_in sockaddr_in;

int InitSockets(int* server_fd, sockaddr_in* address)
{
    int option = 1;

    // Creating socket file descriptor
    if ((*server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == 0) {
        printf("Socket_fd error\n");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option, sizeof(option))) {
        printf("Setsockopt error\n");
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    // Bind the socket to the network address and port
    if (bind(*server_fd, (struct sockaddr *)address, sizeof(*address)) < 0) {
        printf("Bind error\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int Listen(int* server_fd)
{
    // Start listening for connections
    if (listen(*server_fd, 3) < 0) {
        printf("Listen error\n");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    return 0;
}

void SaveClient(int client_fd)
{
    MessageHeader header;
    if (clients_iterator < MAX_CLIENTS) 
    {
        if (receive_message(client_fd, &header, &clients[clients_iterator], sizeof(Client)) < 0)
        {
            printf("Error receiving client info\n");
            return;
        }
        clients[clients_iterator].socket = client_fd;
        printf("Client registered: %s\n", clients[clients_iterator].client_id);
        clients_iterator++;
    } else {
        printf("Max clients reached. Rejecting connection.\n");
        close(client_fd);
    }
}

void SaveAgent(int agent_fd)
{
    MessageHeader header;
    if (agents_iterator < MAX_AGENTS) 
    {
        if(receive_message(agent_fd, &header, &agents[agents_iterator], sizeof(Agent)) < 0)
        {
            printf("Agent info not received\n");
            return;
        }
        agents[agents_iterator].socket = agent_fd;
        agents[agents_iterator].is_busy = 0;
        printf("Agent registered: %s\n", agents[agents_iterator].id);
        agents_iterator++;
    } else {
        printf("Max agents reached. Rejecting connection.\n");
        close(agent_fd);
    }
}

void RemoveAgent(int agent_fd) {
    pthread_mutex_lock(&agents_mutex);
    for (int i = 0; i < agents_iterator; i++) {
        if (agents[i].socket == agent_fd) {
            printf("Removing agent: %s\n", agents[i].id);
            agents[i] = agents[agents_iterator - 1]; // Replace with the last agent
            agents_iterator--;
            break;
        }
    }
    pthread_mutex_unlock(&agents_mutex);
}

void RemoveClient(int client_fd) {
    for (int i = 0; i < clients_iterator; i++) {
        if (clients[i].socket == client_fd) {
            printf("Removing client: %s\n", clients[i].client_id);
            clients[i] = clients[clients_iterator - 1]; // Replace with the last client
            clients_iterator--;
            break;
        }
    }
}

//TODO rezolva functia asta sa mearga 
Agent* find_available_agent(Task* task) 
{
    for(int i = 0; i < agents_iterator; i++) 
    {
        Agent* agent = &agents[i];
        pthread_mutex_lock(&agents[i].lock);
         if (!agent->is_busy &&
            agent->capabilities.memory_mb >= task->min_memory &&
            (!task->requires_gpu || agent->capabilities.has_gpu) &&
            (agent->flags & task->flags) == task->flags) 
        {
            agents[i].is_busy = 1;
            pthread_mutex_unlock(&agents[i].lock);
            return &agents[i];
        }
        pthread_mutex_unlock(&agents[i].lock);
    }
    return NULL;
}

void EnqueueTask(Task* task) {
    pthread_mutex_lock(&queue_mutex);
    
    if (task_count < MAX_TASKS) {
        task_queue[task_count] = *task;
        task_queue[task_count].task_id = task_count;
        task_count++;
        printf("Task enqueued. Task ID: %d\n", task->task_id);
    } else {
        printf("Task queue is full\n");
    }
    
    pthread_mutex_unlock(&queue_mutex);
}

void* ProcessTasks2(void* arg) {
    while (1) {
        Task* current_task = NULL;
        Agent* assigned_agent = NULL;
        TaskResult task_result;
        MessageHeader header;
        
        pthread_mutex_lock(&queue_mutex);
        if (task_count > 0) {
            current_task = malloc(sizeof(Task));
            memcpy(current_task, &task_queue[0], sizeof(Task));
                        
            for (int i = 0; i < task_count - 1; i++) {
                task_queue[i] = task_queue[i + 1];
            }
            task_count--;
        }
        pthread_mutex_unlock(&queue_mutex);
        
        if (current_task != NULL) 
        {
            pthread_mutex_lock(&agents_mutex);
            assigned_agent = &agents[agents_iterator - 1];//find_available_agent(current_task);
            
            if (assigned_agent) 
            {
                printf("Assigning task %d to agent %s\n", current_task->task_id, assigned_agent->id);

                if (send_message(assigned_agent->socket, MSG_TASK_ASSIGN, NULL, 0) < 0) 
                {
                    printf("Error sending header\n");
                } 
                else if (send_message(assigned_agent->socket, MSG_TASK_ASSIGN, current_task, sizeof(Task)) < 0) 
                {
                    printf("Error sending task to agent %s\n", assigned_agent->id);
                    assigned_agent->is_busy = 0; // Reset busy status on error
                } 
                else 
                {
                    // Wait for result from agent
                    TaskResult task_result;
                    MessageHeader header;
                    if (receive_message(assigned_agent->socket, &header, &task_result, sizeof(TaskResult)) >= 0) {
                        pthread_mutex_lock(&result_mutex);
                        if (result_count < MAX_TASKS) 
                        {
                            result_queue[result_count++] = task_result;
                            printf("Task result for task %d received and queued\n", task_result.task_id);
                        }
                        pthread_mutex_unlock(&result_mutex);
                    } 
                    else 
                    {
                        printf("Failed to receive task result from agent\n");
                    }
                    assigned_agent->is_busy = 0; // Mark agent free again
                }
            }
            
            pthread_mutex_unlock(&agents_mutex);
        }
        
        usleep(100000);
    }
    return NULL;
}

void* ProcessTasks(void* arg) {
    while (1) {
        Task* current_task = NULL;
        Agent* assigned_agent = NULL;
        TaskResult task_result;
        MessageHeader header;

        pthread_mutex_lock(&queue_mutex);
        if (task_count > 0) {
            current_task = malloc(sizeof(Task));
            memcpy(current_task, &task_queue[0], sizeof(Task));

            // Mutăm taskurile rămase înainte în coadă
            memmove(&task_queue[0], &task_queue[1], (task_count - 1) * sizeof(Task));
            task_count--;
            printf("[DEBUG] Task dequeued. Task ID: %d, Remaining tasks: %d\n", current_task->task_id, task_count);
            pthread_mutex_unlock(&queue_mutex);

            // Caută un agent disponibil
            pthread_mutex_lock(&agents_mutex);
            assigned_agent = &agents[agents_iterator - 1]; //find_available_agent(current_task);
            if (assigned_agent) {
                assigned_agent->is_busy = 1;
                int agent_socket = assigned_agent->socket;
                pthread_mutex_unlock(&agents_mutex);

                printf("[DEBUG] Task ID: %d assigned to agent: %s\n", current_task->task_id, assigned_agent->id);

                // Trimite task-ul agentului
                if (send_message(agent_socket, MSG_TASK_ASSIGN, NULL, 0) < 0) {
                    printf("[ERROR] Failed to send task header to agent %s\n", assigned_agent->id);
                } else if (send_message(agent_socket, MSG_TASK_ASSIGN, current_task, sizeof(Task)) == 0) {
                    printf("[DEBUG] Task ID: %d sent to agent: %s\n", current_task->task_id, assigned_agent->id);

                    // Așteaptă rezultatul taskului
                    if (receive_message(agent_socket, &header, &task_result, sizeof(TaskResult)) >= 0) {
                        printf("[DEBUG] Task ID: %d result received from agent: %s\n", task_result.task_id, assigned_agent->id);

                        pthread_mutex_lock(&result_mutex);
                        if (result_count < MAX_TASKS) {
                            result_queue[result_count++] = task_result;
                            printf("[DEBUG] Task ID: %d result queued. Total results in queue: %d\n", task_result.task_id, result_count);
                        } else {
                            printf("[ERROR] Result queue is full. Task ID: %d result discarded.\n", task_result.task_id);
                        }
                        pthread_mutex_unlock(&result_mutex);
                    } else {
                        printf("[ERROR] Failed to receive result for Task ID: %d from agent: %s\n", current_task->task_id, assigned_agent->id);
                    }
                } else {
                    printf("[ERROR] Failed to send task ID: %d to agent: %s\n", current_task->task_id, assigned_agent->id);
                }

                // Marchez agentul ca disponibil
                pthread_mutex_lock(&agents_mutex);
                assigned_agent->is_busy = 0;
                pthread_mutex_unlock(&agents_mutex);
                printf("[DEBUG] Agent %s marked as available.\n", assigned_agent->id);
            } else {
                pthread_mutex_unlock(&agents_mutex);
                printf("[DEBUG] No available agent for Task ID: %d. Task requeued.\n", current_task->task_id);

                // Reintroducem taskul în coadă dacă nu există agent disponibil
                pthread_mutex_lock(&queue_mutex);
                if (task_count < MAX_TASKS) {
                    task_queue[task_count++] = *current_task;
                    printf("[DEBUG] Task ID: %d requeued. Total tasks in queue: %d\n", current_task->task_id, task_count);
                } else {
                    printf("[ERROR] Task queue is full. Task ID: %d discarded.\n", current_task->task_id);
                }
                pthread_mutex_unlock(&queue_mutex);
            }

            free(current_task);
        } else {
            pthread_mutex_unlock(&queue_mutex);
        }

        usleep(100000); // Mică pauză pentru a evita supraîncărcarea CPU-ului
    }
    return NULL;
}


void ManageTask(int client_fd) {
    Task task;
    MessageHeader header;

    //receive task from client
    if (receive_message(client_fd, &header, &task, sizeof(Task)) < 0) {
        printf("Error receiving task from client_fd: %d\n", client_fd);
        return;
    }
    
    task.client_socket = client_fd;
    printf("Task: %s\n", task.arguments);
    EnqueueTask(&task);
}

void handle_sigint(int sig)
{
    printf("\nReceived SIGINT (Ctrl+C). Shutting down server...\n");
    const char* shutdown_message = "Server shutting down. Disconnecting...";

    // send shutdown signal to all agents and clients
    for (int i = 0; i < agents_iterator; i++) 
    {
        send_message(agents[i].socket, MSG_SERVER_CLOSE, shutdown_message, strlen(shutdown_message));
        close(agents[i].socket);
        printf("Disconnected agent [%s].\n", agents[i].id);
    }
    for (int i = 0; i < clients_iterator; i++) 
    {
        send_message(clients[i].socket, MSG_SERVER_CLOSE, shutdown_message, strlen(shutdown_message));
        close(clients[i].socket);
        printf("Disconnected client [%s].\n", clients[i].client_id);
    }
    exit(0);
}

void SendResultToClient(int client_fd) {
    MessageHeader header;
    TaskResult result;
    if (receive_message(client_fd, &header, &result, sizeof(TaskResult)) < 0)
    {
        printf("error\n");
        return;
    }

    pthread_mutex_lock(&result_mutex);
    for (int i = 0; i < result_count; i++) {
        if (result_queue[i].task_id == result.task_id) {
            if (send_message(client_fd, MSG_TASK_RESULT, &result_queue[i], sizeof(TaskResult)) >= 0) {
                printf("Result for task %d sent to client %d\n", result.task_id, client_fd);
            } else {
                printf("Failed to send result for task %d to client %d\n", result.task_id, client_fd);
            }

            // Eliminate from queue
            result_queue[i] = result_queue[--result_count];
            pthread_mutex_unlock(&result_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&result_mutex);
    printf("No result found for task %d\n", result.task_id);
}

int main() {
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];
    pthread_t task_thread;

    signal(SIGINT, handle_sigint);
    
    // initialize server
    InitSockets(&server_fd, &address);
    Listen(&server_fd);

    if (pthread_create(&task_thread, NULL, ProcessTasks, NULL) != 0) {
        perror("Failed to create task processing thread");
        exit(EXIT_FAILURE);
    }

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll creation error");
        exit(EXIT_FAILURE);
    }

    event.events = EPOLLIN;
    event.data.fd = server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("Epoll control error");
        exit(EXIT_FAILURE);
    }

    printf("Server ready. Waiting for connections...\n");

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) 
        {
            perror("Epoll wait error");
            break;
        }

        for (int i = 0; i < num_events; i++)  
        {
            if (events[i].data.fd == server_fd) {
                int new_socket = accept(server_fd, NULL, NULL);
                if (new_socket == -1) 
                {
                    perror("Accept error");
                    continue;
                }

                event.events = EPOLLIN;
                event.data.fd = new_socket;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_socket, &event) == -1) 
                {
                    perror("Epoll control error for new socket");
                    close(new_socket);
                    continue;
                }

                printf("New connection accepted: socket %d\n", new_socket);
            } 
            else 
            {
                int client_fd = events[i].data.fd;
                MessageHeader header;

                if (receive_message(client_fd, &header, NULL, 0) < 0) 
                {
                    printf("Connection error on socket %d. Closing.\n", client_fd);
                    RemoveAgent(client_fd);
                    RemoveClient(client_fd);
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    continue;
                }

                switch (header.type) {
                    case MSG_AGENT_REGISTER:
                        SaveAgent(client_fd);
                        break;

                    case MSG_CLIENT_REGISTER:
                        SaveClient(client_fd);
                        break;

                    case MSG_TASK_ASSIGN:
                        ManageTask(client_fd);
                        break;

                    case MSG_TASK_STATUS:
                        SendResultToClient(client_fd);
                    case MSG_ERROR:
                        //TODO creeaza o functie care trimite clientului un mesaj de eroare pentru rezolvarea taskului
                        break;
                    default:
                        printf("Unknown message type from socket %d.\n", client_fd);
                        break;
                }
            }
        }
    }

    close(server_fd);
    close(epoll_fd);
    return 0;
}