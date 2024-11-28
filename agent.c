#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"
#include "protocol.h"

#define PORT 8080
#define BUFFER_SIZE 1024
//#define SERVER_IP "192.168.100.201"
//#define SERVER_IP "192.168.146.83"
#define SERVER_IP "127.0.0.1"

char agent_id[32];
AgentCapabilities capabilities;

void execute_task(Task* task) {
    char cmd[BUFFER_SIZE];
    snprintf(cmd, BUFFER_SIZE, "%s %s", task->executable_path, task->arguments);
    
    FILE* fp = popen(cmd, "r");
    pclose(fp);
}

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

void ExecuteTask(char* command[])
{
    pid_t pid1, pid2;
    int status;
    int redirect_index = -1;
    int pipe_index = -1;
    int pipefd[2];

    for (int i = 0; command[i] != NULL; i++) {
        if (strcmp(command[i], ">") == 0) {
            redirect_index = i;
        }

        if (strcmp(command[i], "|") == 0) {
            pipe_index = i;
        }
    }

    if (pipe_index != -1) {
        if (pipe(pipefd) == -1) {
            perror("pipe error");
            exit(EXIT_FAILURE);
        }
    }

    pid1 = fork();
    if (pid1 == 0) {
        if (pipe_index != -1) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]);  
            close(pipefd[1]);  
        }

        
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

            dup2(fd, STDOUT_FILENO);  
            close(fd);
            command[redirect_index] = NULL; 
        }

        command[pipe_index] = NULL;  
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
            dup2(pipefd[0], STDIN_FILENO);  
            close(pipefd[1]);  
            close(pipefd[0]);

            if (execvp(command[pipe_index + 1], &command[pipe_index + 1]) == -1) {
                perror("execvp error");
                exit(EXIT_FAILURE);
            }
        } else if (pid2 < 0) {
            perror("fork error");
        }
    }

    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, &status, 0);
    if (pipe_index != -1) {
        waitpid(pid2, &status, 0);
    }

    
}

// void agent_main_loop(int socket) {
//     MessageHeader header;
//     char payload_buffer[MAX_PAYLOAD_SIZE];
    
//     // inregistrare agent
//     example_agent_registration(socket, "AGENT001");
    
//     while(1) {
//         int result = receive_message(socket, &header, payload_buffer, MAX_PAYLOAD_SIZE);
//         if (result < 0) {
//             printf("Error receiving message: %d\n", result);
//             break;
//         }
        
//         switch(header.type) {
//             case MSG_TASK_ASSIGN: {
//                 TaskSubmission* task = (TaskSubmission*)payload_buffer;
//                 char result_str[1024] = "Task completed successfully";
//                 example_send_result(socket, header.sequence, result_str);
//                 break;
//             }
//             // Handle other message types...
//         }
//     }
// }

void ParseCommand(char *input, char *command[]) {
    char *token;
    int index = 0;

    token = strtok(input, " ");
    while (token != NULL) {
        command[index] = token;
        index++;
        token = strtok(NULL, " ");
    }
    command[index] = NULL;  // NULL-terminate the array
}

int main(int argc, char* argv[]) {
    printf("Agent starting running...\n");

    if(argc != 2) {
        printf("Usage: %s <agent_id>\n", argv[0]);
        return 1;
    }
    
    strcpy(agent_id, argv[1]);

    printf("AGENT ID: %s\n", agent_id);
    
    capabilities.can_execute_binary = 1;
    capabilities.has_gpu = 0;
    capabilities.memory_mb = 1024;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0){
        printf("Successfull connection with server...\n");
        send(sock, agent_id, strlen(agent_id), 0);
    }
    else{
        printf("Connection error with server...\n");
    }

    
    char buffer[BUFFER_SIZE];
    while(1) {
        int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if(read_size <= 0) break;

        buffer[read_size] = 0;
        printf("Task from server: %s, (len = %d)\n", buffer, read_size);

        char* command[255];

        ParseCommand(buffer, command);

        ExecuteTask(command);
        
        //Task* task = (Task*)buffer;
        //execute_task(task);
    }
    
    return 0;
}