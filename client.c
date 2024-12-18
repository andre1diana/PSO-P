#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>

#include "common.h"
#include "protocol.h"

#define SERVER_IP "10.0.2.15"
#define MENU_OPTIONS 6

void connect_to_server();
void type_task();
void select_task();
void send_task();
void task_status();
void cleanup_and_exit();
void print_menu(int selected);
bool is_executable(const char *filepath);

char task[BUFFER_SIZE];

int main() {
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
                    type_task();
                    break;
                case 2:
                    select_task();
                    break;
                case 3:
                    send_task();
                    break;
                case 4:
                    task_status();
                    break;
                case 5:
                    cleanup_and_exit();
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                    exit(0);
            }
        }
    }

    return 0;
}

void connect_to_server() {
    printf("[Not implemented] Connect to server.\n");
    getchar();
}

void type_task() {
    struct termios oldt, newt;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    printf("Enter the task command: ");
    fgets(task, BUFFER_SIZE, stdin);
    task[strcspn(task, "\n")] = 0;
    printf("Task recorded: %s\n", task);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    getchar();
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
    printf("[Not implemented] Task status.\n");
    getchar();
}

void cleanup_and_exit() {
    printf("Cleaning up resources and exiting.\n");
}

void print_menu(int selected) {
    const char *options[MENU_OPTIONS] = {
        "Connect to server",
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