#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <signal.h>
#include <pthread.h>

#include "common.h"
#include "protocol.h"

Agent agents[MAX_AGENTS];
int agents_iterator = 0;
Task task_queue[MAX_TASKS];
int task_count = 0;
Client clients[MAX_CLIENTS];
int clients_iterator = 0;

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t agents_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct sockaddr_in sockaddr_in;

typedef struct {
    int socket;
    Agent* agent;
} ThreadArgs;

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

int AcceptConnection(int server_fd)
{
    int new_socket;

    if ((new_socket = accept(server_fd, NULL, NULL)) < 0) 
    {
        perror("Accept error");
        return -1;
    }
    return new_socket;
}

int SaveClientAgent(int new_socket, MessageHeader header)
{
    if(header.type == MSG_AGENT_REGISTER)
    {
        if (agents_iterator >= MAX_AGENTS) 
        {
            printf("Agent list full. Connection rejected.\n");
            close(new_socket);
            return -1;
        }
        if(receive_message(new_socket, &header, &agents[agents_iterator], sizeof(Agent)) != 0)
        {
            printf("Could not receive agent info. \n");
            close (new_socket);
            return -1;
        }

        agents[agents_iterator].socket = new_socket;
        printf("Agent connection accepted (%s)...\n", agents[agents_iterator].id);
        agents_iterator++;
    }
    else if(header.type == MSG_CLIENT_REGISTER)
    {
        if (clients_iterator >= MAX_CLIENTS) 
        {
            printf("Clients list full. Connection rejected.\n");
            close(new_socket);
            return -1;
        }
        if(receive_message(new_socket, &header, &clients[clients_iterator], sizeof(Client)) != 0)
        {
            printf("Could not receive client info. \n");
            close (new_socket);
            return -1;
        }
        clients[clients_iterator].socket = new_socket;
        printf("Client connection accepted (%s)...\n", clients[clients_iterator].client_id);
        clients_iterator++;
    }
    return 0;
}

int has_capability(Agent *agent, int capability) {
    return (agent->flags & capability) != 0;
}

void handle_sigint(int sig)
{
    printf("\nReceived SIGINT (Ctrl+C). Shutting down server...\n");
    const char* shutdown_message = "Server shutting down. Disconnecting...";

    // send shutdown signal to all agents and clients
    for (int i = 0; i < agents_iterator; i++) {
        send_message(agents[i].socket, MSG_SERVER_CLOSE, shutdown_message, strlen(shutdown_message));
        close(agents[i].socket);
        printf("Disconnected agent [%s].\n", agents[i].id);
    }
    for (int i = 0; i < clients_iterator; i++) {
        send_message(clients[i].socket, MSG_SERVER_CLOSE, shutdown_message, strlen(shutdown_message));
        close(clients[i].socket);
        printf("Disconnected client [%s].\n", clients[i].client_id);
    }
    exit(0);
}

int main() {
    int server_fd;
    struct sockaddr_in address;
    pthread_t agent_monitor_thread;

    signal(SIGINT, handle_sigint);
    
    // initialize server
    InitSockets(&server_fd, &address);
    Listen(&server_fd);


    printf("Server ready. Waiting for connections...\n");

    while (1) {
        int new_socket = AcceptConnection(server_fd);

        MessageHeader* header = malloc(sizeof(MessageHeader));

        if(receive_message(new_socket, header, NULL, 0) != 0)
        {
            printf("Connection error.\n");
            close (new_socket);
            return -1;
        }

        switch (header->type)
        {
        case MSG_AGENT_REGISTER:
            SaveClientAgent(new_socket, *header);
            break;
        case MSG_CLIENT_REGISTER:
            SaveClientAgent(new_socket, *header);
            break;
        default:
            break;
        }
            
    }

    close(server_fd);
    return 0;
}