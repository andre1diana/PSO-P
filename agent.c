#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.100.201"

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

int ReceiveFile(int socket, const char* file_path) {
    FILE *file = fopen(file_path, "wb");
    if (file == NULL) {
        perror("File open failed");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    int bytes_received;

    // Keep receiving data in chunks
    while ((bytes_received = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        if(strcmp(buffer, "ED") == 0)
        {
            printf("EOF\n");
            return 0;
        }

        // Write the received bytes to the file
        if (fwrite(buffer, 1, bytes_received, file) != bytes_received) {
            perror("File write failed");
            fclose(file);
            return -1;
        }
    }

    if (bytes_received < 0) {
        perror("Receive failed");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

int main() {
    int sock = init_connection();

    char *message = "Hello, Leuuu";
    char buffer[BUFFER_SIZE] = {0};

    send(sock, message, strlen(message), 0);
    printf("Message sent\n");

    // Read the response from the server
    int ok = ReceiveFile(sock, "file");
    if (ok == 0)
    {
        printf("file received");
    }
    //read(sock, buffer, BUFFER_SIZE);
    //printf("Server: %s\n", buffer);

    close(sock);
    return 0;
}