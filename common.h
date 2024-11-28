#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_AGENTS 100
#define MAX_CLIENTS 50
#define MAX_TASKS 200
#define PORT 8080
#define BUFFER_SIZE 1024

// Tipuri de mesaje
enum MessageType {
    REGISTER_AGENT,
    SUBMIT_TASK,
    TASK_RESULT,
    TASK_ASSIGNMENT
};

// Structura pentru specializarile agentului
typedef struct {
    int can_execute_binary;
    int has_gpu;
    int memory_mb;
    // Altele
} AgentCapabilities;

// Structura pentru agent
typedef struct {
    char id[32];
    int socket;
    int is_busy;
    AgentCapabilities capabilities;
    //lista de stringuri cu capabilitati -> labeluri //enum 
    pthread_mutex_t lock;
} Agent;

// Structura pentru task
typedef struct {
    int task_id;
    char executable_path[256];
    char arguments[256];
    int requires_gpu;
    int min_memory;
    int client_socket;
    int is_async;
    // Altele
} Task;



#endif
