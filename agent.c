#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>


#include"task.h"

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

int ReceiveTask(int socket, TASK_DATA task_data)
{

}

int SendResult(int socket, TASK_DATA result)
{

}

void ExecuteTask(char* command[])
{
    pid_t pid1, pid2;
    int status;
    int redirect_index = -1;
    int pipe_index = -1;
    int pipefd[2]; // Descriptorii pentru pipe

    // Detectăm redirecționarea și pipe-urile
    for (int i = 0; command[i] != NULL; i++) {
        if (strcmp(command[i], ">") == 0) {
            redirect_index = i;
        }

        if (strcmp(command[i], "|") == 0) {
            pipe_index = i;
        }
    }

    // Creăm un pipe dacă e nevoie
    if (pipe_index != -1) {
        if (pipe(pipefd) == -1) {
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
    }

    pid1 = fork();
    if (pid1 == 0) {
        // Proces copil pentru comanda din stânga pipe-ului
        if (pipe_index != -1) {
            // Redirecționăm output-ul comenzii în pipe
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);  // Închidem capătul de citire al pipe-ului
            close(pipefd[1]);  // Nu mai avem nevoie de capătul de scriere al pipe-ului
        }

        // Dacă există redirecționare de output, tratăm asta
        if (redirect_index != -1) {
            char *filename = command[redirect_index + 1];
            if (filename == NULL) {
                fprintf(stderr, "Expected filename after '>'\n");
                exit(EXIT_FAILURE);
            }

            int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd < 0) {
                perror("open error");
                exit(EXIT_FAILURE);
            }

            dup2(fd, STDOUT_FILENO);  // Redirecționăm stdout în fișier
            close(fd);
            command[redirect_index] = NULL;  // Tăiem comanda la poziția de redirecționare
        }

        // Executăm prima comandă
        command[pipe_index] = NULL;  // Tăiem comanda la poziția pipe-ului
        if (execvp(command[0], command) == -1) {
            perror("execvp error");
            exit(EXIT_FAILURE);
        }
    } else if (pid1 < 0) {
        perror("fork error");
    }

    if (pipe_index != -1) {
        pid2 = fork();
        if (pid2 == 0) {
            // Proces copil pentru comanda din dreapta pipe-ului
            dup2(pipefd[0], STDIN_FILENO);  // Redirecționăm input-ul din pipe
            close(pipefd[1]);  // Închidem capătul de scriere al pipe-ului
            close(pipefd[0]);  // Închidem capătul de citire al pipe-ului după dup2

            // Executăm a doua comandă
            if (execvp(command[pipe_index + 1], &command[pipe_index + 1]) == -1) {
                perror("execvp error");
                exit(EXIT_FAILURE);
            }
        } else if (pid2 < 0) {
            perror("fork error");
        }
    }

    // În procesul părinte, așteptăm ambele procese să se termine
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, &status, 0);
    if (pipe_index != -1) {
        waitpid(pid2, &status, 0);
    }
}

int main() {
/*     int sock = init_connection();

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

    close(sock); */

    char *command[] = {"ls", "-l", NULL};

    ExecuteTask(command);

    return 0;
}