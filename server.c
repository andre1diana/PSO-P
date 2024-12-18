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

int AcceptConnectionAgent(int server_fd, Agent* new_agent)
{
    int new_socket;
    char buffer[MAX_PAYLOAD_SIZE];
    if ((new_socket = accept(server_fd, NULL, NULL)) < 0) {
        printf("Accept error");
        exit(EXIT_FAILURE);
    }
    else{
        MessageHeader header;
        printf("DEBUG agent accepted\n");
        while(1){
            int recv_size = receive_message(new_socket, &header, &(agents[agents_iterator]), MAX_PAYLOAD_SIZE);
            if(recv_size < 0)
            {
                printf("Connection error. \n");
            }
            agents_iterator++;
            printf("Agent connection accepted (%d, %s)...\n",new_socket, buffer);
        }
    }

    return 0;
}

int has_capability(Agent *agent, int capability) {
    return (agent->flags & capability) != 0;
}


int main() {
    int server_fd;
    struct sockaddr_in address;
    
    // initialize server
    InitSockets(&server_fd, NULL, &address);
    Listen(&server_fd, &address);


    close(server_fd);
    return 0;
}