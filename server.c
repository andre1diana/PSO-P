#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <asm-generic/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024

typedef struct sockaddr_in sockaddr_in;

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

int main() {
    int server_fd, new_socket;
    sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char *hello = "Hello Didiii\n";

    InitSockets(&server_fd, &new_socket, &address);
    Listen(&server_fd, &address);

    while (1)
{
    // Accept a connection from the client

    AcceptConnection(server_fd, &new_socket, address);

    // Read the message from the client
    ReceiveMessage(new_socket, buffer);

    memset(buffer, 0, BUFFER_SIZE);

    // Send a message to the client
    SendMessage(new_socket, hello);

    SendFile(new_socket, "ToSend.txt");
}

    close(new_socket);
    close(server_fd);
    return 0;
}