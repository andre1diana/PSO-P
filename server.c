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
#include <stdbool.h>

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

bool is_client_registered(const char* client_id) {
    for(int i = 0; i < clients_iterator; i++) {
        if(strcmp(clients[i].client_id, client_id) == 0) {
            printf("[DEBUG] Client %s is already registered\n", client_id);
            return true;
        }
    }
    printf("[DEBUG] Client %s is not registered yet\n", client_id);
    return false;
}

void SaveClient(int client_fd) {
    MessageHeader header;
    Client temp_client;
    
    if(receive_message(client_fd, &header, &temp_client, sizeof(Client)) < 0) {
        printf("Error receiving client info\n");
        return;
    }

    if(is_client_registered(temp_client.client_id)) {
        printf("Client with ID %s already exists. Rejecting connection.\n", temp_client.client_id);
        close(client_fd);
        return;
    }

    if (clients_iterator < MAX_CLIENTS) {
        memcpy(&clients[clients_iterator], &temp_client, sizeof(Client));
        clients[clients_iterator].socket = client_fd;
        printf("Client registered: %s\n", clients[clients_iterator].client_id);
        clients_iterator++;
    } else {
        printf("Max clients reached. Rejecting connection.\n");
        close(client_fd);
    }
}

bool is_agent_registered(const char* agent_id) 
{
    for(int i = 0; i < agents_iterator; i++) {
        if(strcmp(agents[i].id, agent_id) == 0) {
            return true;
        }
    }
    return false;
}

void SaveAgent(int agent_fd)
{
    MessageHeader header;
    Agent temp_agent;

    if(receive_message(agent_fd, &header, &temp_agent, sizeof(Agent)) < 0)
    {
        printf("Agent info not received\n");
        return;
    }

    if(is_agent_registered(temp_agent.id)) 
    {
        printf("Agent with ID %s already exists. Rejecting connection.\n", temp_agent.id);
        close(agent_fd);
        return;
    }

    if (agents_iterator < MAX_AGENTS) {
        memcpy(&agents[agents_iterator], &temp_agent, sizeof(Agent));
        agents[agents_iterator].socket = agent_fd;
        agents[agents_iterator].is_busy = 0;
        printf("Agent registered: %s\n", agents[agents_iterator].id);
        agents_iterator++;
        printf("Agent info : %s == %d == %d == %d\n", 
            agents[agents_iterator - 1].id, 
            agents[agents_iterator - 1].capabilities.memory_mb, 
            agents[agents_iterator - 1].capabilities.has_gpu, 
            agents[agents_iterator - 1].flags);
    }
    else 
    {
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

Agent* find_available_agent(Task* task) 
{
    for(int i = 0; i < agents_iterator; i++) 
    {
        Agent* agent = &agents[i];
        
        printf("\n[DEBUG] Checking agent %s:\n", agent->id);
        printf("- Busy: %d\n", agent->is_busy);
        printf("- Memory: %d MB\n", agent->capabilities.memory_mb);
        printf("- Has GPU: %d\n", agent->capabilities.has_gpu);
        printf("- Flags: %d\n", agent->flags);

        if (!agent->is_busy &&
            agent->capabilities.memory_mb >= task->min_memory &&
            (!task->requires_gpu || agent->capabilities.has_gpu) &&
            (agent->flags & task->flags) == task->flags) 
        {
            printf("[DEBUG] Found suitable agent: %s\n", agent->id);
            agents[i].is_busy = 1;
            return &agents[i];
        }
    }
    printf("[DEBUG] No suitable agent found\n");
    return NULL;
}

void EnqueueTask(Task* task) {
    pthread_mutex_lock(&queue_mutex);
    
    if (task_count < MAX_TASKS) {
        task_queue[task_count] = *task;
        task_count++;
        printf("Task enqueued. Task ID: %d\n", task->task_id);
    } else {
        printf("Task queue is full\n");
    }
    
    pthread_mutex_unlock(&queue_mutex);
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

            // Move remaining tasks up in the queue
            memmove(&task_queue[0], &task_queue[1], (task_count - 1) * sizeof(Task));
            task_count--;
            printf("[DEBUG] Task dequeued. Task ID: %d, Remaining tasks: %d\n", current_task->task_id, task_count);
            pthread_mutex_unlock(&queue_mutex);

            // Find an available agent for the task
            pthread_mutex_lock(&agents_mutex);
            assigned_agent = find_available_agent(current_task);
            if (assigned_agent) {
                assigned_agent->is_busy = 1;
                int agent_socket = assigned_agent->socket;
                pthread_mutex_unlock(&agents_mutex);

                printf("[DEBUG] Task ID: %d assigned to agent: %s\n", current_task->task_id, assigned_agent->id);

                // Send task to agent
                if (send_message(agent_socket, MSG_TASK_ASSIGN, NULL, 0) < 0) {
                    printf("[ERROR] Failed to send task header to agent %s\n", assigned_agent->id);
                } else if (send_message(agent_socket, MSG_TASK_ASSIGN, current_task, sizeof(Task)) == 0) {
                    printf("[DEBUG] Task ID: %d sent to agent: %s\n", current_task->task_id, assigned_agent->id);

                    // Wait for result from agent
                    if (receive_message(agent_socket, &header, &task_result, sizeof(TaskResult)) >= 0) {
                        printf("[DEBUG] Task ID: %d result received from agent: %s\n", task_result.task_id, assigned_agent->id);
                        task_result.client_sock = current_task->client_socket;

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

                // Mark agent as available
                pthread_mutex_lock(&agents_mutex);
                assigned_agent->is_busy = 0;
                pthread_mutex_unlock(&agents_mutex);
                printf("[DEBUG] Agent %s marked as available.\n", assigned_agent->id);
            } else {
                pthread_mutex_unlock(&agents_mutex);
                printf("[DEBUG] No available agent for Task ID: %d. Task requeued.\n", current_task->task_id);

                // Put task back in the queue
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

        usleep(100000); // Small pause to avoid CPU overload
    }
    return NULL;
}


void ManageTask(int client_fd) {
    Task task;
    MessageHeader header;

    //Receive task from client
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

    // Send shutdown signal to all agents and clients
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
    pthread_mutex_lock(&result_mutex);

    int sent_count = 0;
    printf("Results count: %d\n", result_count);
    for (int i = 0; i < result_count; ) {
        printf("Client socket: %d\n", client_fd);
        printf("Client socket: %d\n", result_queue[i].client_sock);
        if (result_queue[i].client_sock == client_fd) {
            printf("Am fost aici\n");
            if(send_message(client_fd, MSG_TASK_RESULT, NULL, 0) < 0)
            {
                printf("Error sending the header\n");   
            }
            else if (send_message(client_fd, MSG_TASK_RESULT, &result_queue[i], sizeof(TaskResult)) >= 0) {
                printf("Result for task %d sent to client (fd: %d)\n", result_queue[i].task_id, client_fd);
                sent_count++;
            } else {
                printf("Failed to send result for task %d to client (fd: %d)\n", result_queue[i].task_id, client_fd);
            }

            result_queue[i] = result_queue[--result_count];
        } else {
            i++;
        }
    }

    pthread_mutex_unlock(&result_mutex);

    if (sent_count == 0) {
        printf("No results found for client\n");
        TaskResult res = {0};
        res.status_code = -100;
        send_message(client_fd, MSG_TASK_RESULT, NULL, 0);
        send_message(client_fd, MSG_TASK_RESULT, &res, sizeof(TaskResult));
        return;
    } else {
        printf("Total %d results sent to client\n", sent_count);
    }
    send_message(client_fd, MSG_TASK_COMPLETE, NULL, 0);
}

int main() {
    int server_fd, epoll_fd;
    struct sockaddr_in address;
    struct epoll_event event, events[MAX_EVENTS];
    pthread_t task_thread;

    signal(SIGINT, handle_sigint);
    
    // Initialize server
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
                        //TODO Make a function that sends a message to the client with an error for the task resolution
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