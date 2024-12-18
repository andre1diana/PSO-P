#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "10.0.2.15"

int init_connection(){
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed\n");
        return -1;
    }
    return sock;
}

int main()
{
    int sock = init_connection();

    char *message = "Hello, Leuuu";
    char buffer[BUFFER_SIZE] = {0};

    send(sock, message, strlen(message), 0);
    printf("Message sent\n");

    //read(sock, buffer, BUFFER_SIZE);
    //printf("Server: %s\n", buffer);

    close(sock);
}