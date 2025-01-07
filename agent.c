#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "common.h"
#include "protocol.h"

char agent_id[32];
AgentCapabilities capabilities;
int flags;


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

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
       printf("Successfull connection with server...\n");
        Agent *agent = malloc(sizeof(Agent));
        strcpy(agent->id, agent_id);
        agent->capabilities = capabilities;
        agent->flags = flags;
        agent->is_busy = 0;
        if (send_message(sock, MSG_AGENT_REGISTER, NULL, 0) != 0)
        {
            printf("Could not send agent info");
        }
        if (send_message(sock, MSG_AGENT_REGISTER, agent, sizeof(agent)) != 0)
        {
            printf("Could not send agent info");
        }
    }
    else{
        printf("Connection error with server...\n");
        exit(-1);
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

//TODO 1 schimba aici sa poti trimite rezultatul serverului
void ExecuteTask2(char* command[])
{
    printf("Start executing task\n");
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

//second function for execute task
void ExecuteTask(char* command[], char** result) {
    char temp_file[] = "/tmp/task_result_XXXXXX";
    int fd = mkstemp(temp_file); // Creează fișier temporar
    if (fd == -1) {
        perror("mkstemp error");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Proces copil
        dup2(fd, STDOUT_FILENO); // Redirecționează stdout către fișier
        dup2(fd, STDERR_FILENO); // (opțional) Redirecționează și stderr
        close(fd);

        if (execvp(command[0], command) == -1) {
            perror("execvp error");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {
        // Proces părinte
        close(fd); // Închide descriptorul în părinte
        wait(NULL); // Așteaptă copilul să termine

        // Citește conținutul fișierului
        FILE* file = fopen(temp_file, "r");
        if (!file) {
            perror("fopen error");
            exit(EXIT_FAILURE);
        }

        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        rewind(file);

        *result = malloc(size + 1);
        if (!*result) {
            perror("malloc error");
            exit(EXIT_FAILURE);
        }

        fread(*result, 1, size, file);
        (*result)[size] = '\0';

        fclose(file);

        // Șterge fișierul temporar
        unlink(temp_file);
    } else {
        perror("fork error");
    }
}

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

void initializeAgent(const char* agentFile)
{
    FILE* file = fopen(agentFile, "r");
    if (file == NULL) {
        perror("Failed to open agent file");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Ignor liniile care sunt comentarii sau goale
        if (line[0] == '#' || strlen(line) <= 1) {
            continue;
        }

        // Read values from file
        sscanf(line, "%31[^,], %d, %d, %d, %d",
               agent_id,
               &capabilities.can_execute_binary,
               &capabilities.has_gpu,
               &capabilities.memory_mb,
               &flags);

        // Afisam datele pentru verificare
        printf("Agent ID: %s\n", agent_id);
        printf("  Can Execute Binary: %d\n", capabilities.can_execute_binary);
        printf("  Has GPU: %d\n", capabilities.has_gpu);
        printf("  Memory (MB): %d\n", capabilities.memory_mb);
        printf("  Flags: %d\n", flags);
    }

    fclose(file);
}

void ProcessTask(int sock, Task* task) {
    printf("Received task ID: %d\n", task->task_id);
    
    // Task validation
    if (task->min_memory > capabilities.memory_mb) {
        printf("Error: Task requires more memory than available\n");
        const char* error_msg = "Insufficient memory";
        //send_message(sock, MSG_ERROR, error_msg, strlen(error_msg));
        return;
    }

    if (task->requires_gpu && !capabilities.has_gpu) {
        printf("Error: Task requires GPU but none available\n");
        const char* error_msg = "GPU not available";
        //send_message(sock, MSG_ERROR, error_msg, strlen(error_msg));
        return;
    }

    printf("Executing task with arguments: %s\n", task->arguments);

    // Parse command
    char *command[256];
    ParseCommand(task->arguments, command);

    //execute command
    char* result = NULL;
    ExecuteTask(command, &result);

    TaskResult task_result = {
        .task_id = task->task_id,
        .status_code = 0
    };
    strncpy(task_result.result, result, sizeof(task_result.result) - 1);
    task_result.result[sizeof(task_result.result) - 1] = '\0';
    
    if (send_message(sock, MSG_TASK_COMPLETE, &task_result, sizeof(TaskResult)) < 0) {
        printf("Failed to send task completion message\n");
    } else {
        printf("Task result sent successfully for task %d\n", task->task_id);
    }
    
    free(result);
}

int main(int argc, char* argv[]) {
    printf("Agent starting running...\n");
    
    if(argc != 2) {
        printf("Usage: %s <agent_file>\n", argv[0]);
        return 1;
    }    

    initializeAgent(argv[1]);
    int sock = init_connection();

    MessageHeader header;
    while(1) {
        // Primeste header-ul mesajului
        if (receive_message(sock, &header, NULL, 0) < 0) {
            printf("Error receiving message header\n");
            break;
        }

        switch (header.type) {
            case MSG_TASK_ASSIGN: {
                Task task;
                if (receive_message(sock, &header, &task, sizeof(Task)) < 0) {
                    printf("Error receiving task data\n");
                    continue;
                }
                printf("Argumente task : %s", task.arguments);
                ProcessTask(sock, &task);
                break;
            }
            
            case MSG_SERVER_CLOSE:
                printf("Server shutting down. Closing agent...\n");
                close(sock);
                return 0;
                
            case MSG_ERROR:
                //char error_msg[256];
                //if (receive_message(sock, &header, error_msg, sizeof(error_msg)) < 0) {
                //    printf("Error receiving error message\n");
                //    continue;
                //}
                //printf("Received error from server: %s\n", error_msg);
                break;
                
            default:
                printf("Unknown message type: %d\n", header.type);
                break;
        }
    }
    
    close(sock);
    return 0;
}