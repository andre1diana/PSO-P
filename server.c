#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>

#include "common.h"
#include "protocol.h"

#define PORT 8080
#define BUFFER_SIZE 1024

// Structuri globale pentru server
Agent agents[MAX_AGENTS];
Task task_queue[MAX_TASKS];
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
int num_agents = 0;
int task_count = 0;

typedef struct sockaddr_in sockaddr_in;

// Functie pentru gasirea unui agent disponibil
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

// Handler pentru conexiuni noi
void* handle_connection(void* socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[BUFFER_SIZE];
    
    while(1) {
        int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if(read_size <= 0) break;
        
        // Procesare mesaj...
        // Implementare pentru diferite tipuri de mesaje
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

int AcceptConnection(int server_fd, int *new_socket, sockaddr_in address)
{
    int addrlen = sizeof(address);

    if ((*new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        printf("Accept error");
        exit(EXIT_FAILURE);
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

    // Buffer to store file data to be sent
    char buffer[1024];
    int bytes_read;

    // Send the file in chunks
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

int main() {
    int server_fd;
    struct sockaddr_in address;
    pthread_t thread_id;
    
    // Initializare socket
    InitSockets(server_fd, socket, &address);
    Listen(server_fd, &address);
    
    while(1) {
        int new_socket = accept(server_fd, NULL, NULL);
        pthread_create(&thread_id, NULL, handle_connection, (void*)&new_socket);
        pthread_detach(thread_id);
    }
    
    return 0;
}