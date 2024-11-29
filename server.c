#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>

#include "common.h"
#include "protocol.h"

#define PORT 8080
#define BUFFER_SIZE 1024

Agent agents[MAX_AGENTS];
int agents_iterator = 0;
Task task_queue[MAX_TASKS];
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int num_agents = 0;
int task_count = 0;

typedef struct sockaddr_in sockaddr_in;

Agent* find_available_agent(Task* task) {
    for(int i = 0; i < num_agents; i++) {
        pthread_mutex_lock(&agents[i].lock);
        if (!agents[i].is_busy && 
            agents[i].capabilities.memory_mb >= task->min_memory &&
            (!task->requires_gpu || agents[i].capabilities.has_gpu)) {
            agents[i].is_busy = 1;
            pthread_mutex_unlock(&agents[i].lock);
            return &agents[i];
        }
        pthread_mutex_unlock(&agents[i].lock);
    }
    return NULL;
}

void* handle_connection(void* socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    
    while(1) {
        int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if(read_size <= 0) {
            break;
        }
    }
    
    return NULL;
}

int InitSockets(int* server_fd, int* new_socket, sockaddr_in* address)
{
    int option = 1;
    int addrlen = sizeof(address);

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

int Listen(int* server_fd, sockaddr_in* address)
{
    // Start listening for connections
    if (listen(*server_fd, 3) < 0) {
        printf("Listen error\n");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    return 0;
}

int AcceptConnection(int server_fd, Agent* new_agent)
{
    printf("DEBUG1\n");
    int new_socket;
    char buffer[MAX_PAYLOAD_SIZE];
    if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
        printf("Accept error");
        exit(EXIT_FAILURE);
    }
    else{
        printf("DEBUG agent accepted\n");
        while(1){
            int recv_size = recv(new_socket, buffer, MAX_PAYLOAD_SIZE, 0);
            if(recv_size > 0)
            {
                printf("Agent connection accepted (%d, %s)...\n",new_socket, buffer);
                agents[agents_iterator].socket = new_socket;
                strcpy(agents[agents_iterator].id,buffer);
                agents[agents_iterator].is_busy = 0;
                agents_iterator++;
                printf("DEBUG1\n");
                break;
            }
        }
    }

    return 0;
}

int SendMessage(int new_socket, char* message)
{
    send(new_socket, message, strlen(message), 0);
    printf("Hello message sent\n");

    return 0;
}

int ReceiveMessage(int new_socket, char* buffer)
{
    read(new_socket, buffer, BUFFER_SIZE);
    printf("Client: %s\n", buffer);

    return 0;
}

int SendFile(int new_socket, const char* file_path) {
    FILE *file = fopen(file_path, "rb");
    if (file == NULL) {
        perror("File open failed");
        return -1;
    }

    char buffer[1024];
    int bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(new_socket, buffer, bytes_read, 0) == -1) {
            perror("Send failed");
            fclose(file);
            return -1;
        }
    }

    char* ED = "ED";
    SendMessage(new_socket, ED);

    printf("File sent\n");
    fclose(file);
    return 0;
}

void handle_client_message(int socket) {
    MessageHeader header;
    char payload_buffer[MAX_PAYLOAD_SIZE];
    
    int result = receive_message(socket, &header, payload_buffer, MAX_PAYLOAD_SIZE);
    if (result < 0) {
        return;
    }
    
    switch(header.type) {
        case MSG_AGENT_REGISTER: {
            AgentRegistration* reg = (AgentRegistration*)payload_buffer;
            // Procesează înregistrarea agentului
            break;
        }
        case MSG_TASK_SUBMIT: {
            TaskSubmission* task = (TaskSubmission*)payload_buffer;
            // Procesează task-ul primit
            break;
        }
        case MSG_TASK_RESULT: {
            TaskResult* result = (TaskResult*)payload_buffer;
            // Procesează rezultatul
            break;
        }
    }
}

void MonitorAgents() {
    for (int i = 0; i < agents_iterator; i++) {
        int agent_socket = agents[i].socket;
        const char* agent_id = agents[i].id;
        char buffer[1024];

        int recv_size = recv(agent_socket, buffer, sizeof(buffer), MSG_DONTWAIT);

        if (recv_size == 0) {
            printf("Agent [%s] disconnected gracefully.\n", agent_id);
            close(agent_socket);


            for (int j = i; j < agents_iterator - 1; j++) {
                agents[j] = agents[j + 1];
            }
            agents_iterator--; 
            i--;     
        } else if (recv_size < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            } else if (errno == ECONNRESET) {
                printf("Agent [%s] disconnected unexpectedly (connection reset).\n", agent_id);
                close(agent_socket);

                for (int j = i; j < agents_iterator - 1; j++) {
                    agents[j] = agents[j + 1];
                }
                agents_iterator--;
                i--;
            } else {
                printf("Error receiving from agent [%s]: %s\n", agent_id, strerror(errno));
                close(agent_socket);

                for (int j = i; j < agents_iterator - 1; j++) {
                    agents[j] = agents[j + 1];
                }
                agents_iterator--;
                i--;
            }
        } else {
            buffer[recv_size] = '\0';
            printf("Received from agent [%s]: %s\n", agent_id, buffer);
        }
    }
}



int main() {
    int server_fd;
    struct sockaddr_in address;
    pthread_t thread_id;
    
    // Initializare socket
    InitSockets(&server_fd, &socket, &address);
    Listen(&server_fd, &address);

    int test_command_bool = 1;
    
    while(1) {
        AcceptConnection(server_fd, NULL);
        //pthread_create(&thread_id, NULL, handle_connection, (void*)&new_socket);
        //pthread_detach(thread_id);
        MonitorAgents();

        if(test_command_bool && (agents_iterator != 0))
        {
            test_command_bool = 0;
            char command_buffer[] = "ls -l"; 
            send(agents[0].socket, command_buffer, strlen(command_buffer), 0);
            printf("Command %s sent to (%d %s)...\n", command_buffer, agents[0].socket, agents[0].id);
        }
    }
    
    return 0;
}