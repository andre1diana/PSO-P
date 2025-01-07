#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>

#include "common.h"
#include "protocol.h"

#define MENU_OPTIONS 7

void connect_to_server();
void automatic_task();
void type_task();
void select_task();
void send_task();
void task_status();
void cleanup_and_exit();
void print_menu(int selected);
bool is_executable(const char *filepath);

char task_buff[BUFFER_SIZE];
int client_socket = -1;
char clientID[32];

int main(int argc, const char* argv[]) {

    if(argc != 2)
    {
        printf("Usage: %s <client_id>\n", argv[0]);
        return 0;
    }
    strcpy(clientID, argv[1]);

    int selected = 0;
    char key;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (1) {
        system("clear");
        printf("Task Processing Client\n");
        printf("========================\n");
        print_menu(selected);

        key = getchar();

        if (key == '\033') {
            getchar();
            key = getchar();
            if (key == 'A' && selected > 0) selected--;
            else if (key == 'B' && selected < MENU_OPTIONS - 1) selected++;
        } else if (key == '\n') {
            switch (selected) {
                case 0:
                    connect_to_server();
                    break;
                case 1:
                    //automatic_task();
                    break;
                case 2:
                    type_task();
                    break;
                case 3:
                    select_task();
                    break;
                case 4:
                    send_task();
                    break;
                case 5:
                    task_status();
                    break;
                case 6:
                    cleanup_and_exit();
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                    exit(0);
            }
        }
        MessageHeader header;
        void* payload;
        if (receive_message(client_socket, &header, payload, MAX_PAYLOAD_SIZE) == 0)
        {
            if(header.type == MSG_SERVER_CLOSE)
            {
                printf("Server deconected.\n");
                getchar();
            }
        }
    }

    return 0;
}

void connect_to_server() {

    struct sockaddr_in server_address;

    // Create a socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating socket");
        return;
    }

    // Configure server address
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid server address");
        close(client_socket);
        return;
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection to server failed");
        close(client_socket);
        return;
    }
    else
    {
        printf("Connected to server at %s:%d\n", SERVER_IP, PORT);

        // Set socket to non-blocking mode
        int flags = fcntl(client_socket, F_GETFL, 0);
        if (flags == -1) {
            perror("fcntl F_GETFL failed");
            close(client_socket);
            return;
        }
        if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1) {
            perror("fcntl F_SETFL failed");
            close(client_socket);
            return;
        }
        
        Client *client = malloc(sizeof(Client));
        strcpy(client->client_id, clientID);
        
        //send header first
        if ( send_message(client_socket, MSG_CLIENT_REGISTER, NULL, 0) < 0)
        {
            printf("Could not send message.\n");
            return;
        }
        else
        {
            printf("Header sent succesfully\n");
        }

        if (send_message(client_socket, MSG_CLIENT_REGISTER, client, sizeof(Client)) < 0)
        {
            printf("Could not send client info.\n");
            return;
        }
        else
        {
            printf("Msg sent succesfully\n");
        }
        free(client);
    }
    
    printf("Press any key...");
    getchar();
}

int GenerateTaskID() {
    time_t now = time(NULL);
    return (int)now;
}

void type_task() {
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    char task_buff[256];
    Task* task = malloc(sizeof(Task));
    if (task == NULL) {
        perror("Memory allocation failed");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return;
    }

    printf("Enter the task command: ");
    fgets(task->arguments, sizeof(task->arguments), stdin);
    task->arguments[strcspn(task->arguments, "\n")] = 0;
    //fgets(task_buff, BUFFER_SIZE, stdin);
    //task_buff[strcspn(task_buff, "\n")] = 0;
    printf("Task recorded: %s\n", task_buff);
    //scanf("%s", &task->arguments);
    //strcpy(task->arguments, task_buff);

    printf("Does the task require GPU? (1 for Yes, 0 for No): ");
    scanf("%d", &task->requires_gpu);

    printf("Enter the minimum memory required (MB): ");
    scanf("%d", &task->min_memory);

    printf("Enter any flags (integer value): ");
    scanf("%d", &task->flags);

    printf("Should the task run asynchronously? (1 for Yes, 0 for No): ");
    scanf("%d", &task->is_async);

    task->task_id = GenerateTaskID();

    printf("PRESS ENTER to sent task\nPress anything else to leave...\n");
    char key = getchar();
    if(key == '\n')
    {
        //first send the header
        if (send_message(client_socket, MSG_TASK_ASSIGN, NULL, 0) < 0) {
            perror("Failed to send task to client");
        }

        if (send_message(client_socket, MSG_TASK_ASSIGN, task, sizeof(Task)) < 0) {
            perror("Failed to send task to client");
        } else {
            printf("Task with id %d sent to server successfully.\n", task->task_id);
        }
    }

    free(task);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    getchar(); // Consuma newline-ul ramas
}

void select_task() {
    char command[512];
    char filepath[256];

    snprintf(command, sizeof(command), "zenity --file-selection --title=\"Select a Task File\"");
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error opening file selection dialog");
        return;
    }

    if (fgets(filepath, sizeof(filepath), fp) != NULL) {
        filepath[strcspn(filepath, "\n")] = 0;
        if (is_executable(filepath)) {
            printf("File selected: %s\n", filepath);
        } else {
            printf("Error: The selected file is not an executable.\n");
        }
    } else {
        printf("No file selected.\n");
    }

    pclose(fp);
    getchar();
}

bool is_executable(const char *filepath) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        return false;
    }

    unsigned char magic[4];
    read(fd, magic, 4);
    close(fd);

    return magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F';
}

void send_task() {
    printf("[Not implemented] Send task.\n");
    getchar();
}

void task_status() {
    printf("Enter Task ID to check status: ");
    int task_id;
    scanf("%d", &task_id);
    if (send_message(client_socket, MSG_TASK_STATUS, NULL, 0) < 0) {
        printf("Failed to request task status\n");
        return;
    }
    else if (send_message(client_socket, MSG_TASK_STATUS, &task_id, sizeof(int)) < 0) {
        printf("Failed to request task status\n");
        return;
    }

    MessageHeader header;
    TaskResult result;
    if (receive_message(client_socket, &header, &result, sizeof(TaskResult)) >= 0) {
        //if (header.type == MSG_TASK_RESULT) {
            printf("Task %d result:\n%s\n", result.task_id, result.result);
        //} else {
            //printf("Unexpected message type\n");
        //}
    } else {
        printf("Failed to receive task status\n");
    }
    getchar();
}

void cleanup_and_exit() {
    printf("Cleaning up resources and exiting.\n");
    if (client_socket >= 0) {
        close(client_socket);
        printf("Socket closed.\n");
    }
}

void print_menu(int selected) {
    const char *options[MENU_OPTIONS] = {
        "Connect to server",
        "Automatic task",
        "Type task",
        "Select task",
        "Send task",
        "Task status",
        "Exit"
    };

    for (int i = 0; i < MENU_OPTIONS; i++) {
        if (i == selected) printf("> %s\n", options[i]);
        else printf("  %s\n", options[i]);
    }
}