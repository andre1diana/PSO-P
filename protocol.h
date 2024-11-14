#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Dimensiuni maxime pentru diferite câmpuri
#define MAX_PAYLOAD_SIZE 4096
#define MAX_COMMAND_SIZE 256
#define MAX_ARGS_SIZE 1024

// Tipuri de mesaje pentru identificarea conținutului
typedef enum {
    MSG_AGENT_REGISTER = 1,
    MSG_AGENT_HEARTBEAT,
    MSG_TASK_SUBMIT,
    MSG_TASK_ASSIGN,
    MSG_TASK_RESULT,
    MSG_TASK_STATUS,
    MSG_ERROR
} MessageType;

// Header comun pentru toate mesajele
typedef struct {
    uint32_t magic;         // Magic number pentru verificare (0xDEADBEEF)
    uint32_t version;       // Versiunea protocolului
    MessageType type;       // Tipul mesajului
    uint32_t sequence;      // Număr de secvență pentru tracking
    uint32_t payload_size;  // Dimensiunea payload-ului
    uint32_t checksum;      // Checksum pentru validare
} MessageHeader;

// Structuri pentru diferite tipuri de mesaje
typedef struct {
    char agent_id[32];
    uint32_t capabilities_flags;
    uint32_t memory_available;
} AgentRegistration;

typedef struct {
    char command[MAX_COMMAND_SIZE];
    char arguments[MAX_ARGS_SIZE];
    uint32_t requirements_flags;
    uint32_t timeout;
} TaskSubmission;

typedef struct {
    uint32_t task_id;
    int32_t status_code;
    char result[MAX_PAYLOAD_SIZE];
} TaskResult;

// Funcții pentru serializare/deserializare
uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    
    for(size_t i = 0; i < size; i++) {
        checksum = (checksum << 8) | (checksum >> 24);
        checksum += bytes[i];
    }
    
    return checksum;
}

// Funcție pentru trimiterea unui mesaj complet
int send_message(int socket, MessageType type, const void* payload, size_t payload_size) {
    static uint32_t sequence = 0;
    MessageHeader header = {
        .magic = 0xDEADBEEF,
        .version = 1,
        .type = type,
        .sequence = ++sequence,
        .payload_size = payload_size,
        .checksum = calculate_checksum(payload, payload_size)
    };
    
    // Trimite header
    if (send(socket, &header, sizeof(header), MSG_NOSIGNAL) != sizeof(header)) {
        return -1;
    }
    
    // Trimite payload dacă există
    if (payload_size > 0 && payload != NULL) {
        if (send(socket, payload, payload_size, MSG_NOSIGNAL) != payload_size) {
            return -1;
        }
    }
    
    return 0;
}

// Funcție pentru primirea unui mesaj complet
int receive_message(int socket, MessageHeader* header, void* payload, size_t max_payload_size) {
    // Primește header
    ssize_t received = recv(socket, header, sizeof(MessageHeader), MSG_WAITALL);
    if (received != sizeof(MessageHeader)) {
        return -1;
    }
    
    // Verifică magic number
    if (header->magic != 0xDEADBEEF) {
        return -2;
    }
    
    // Verifică dimensiunea payload-ului
    if (header->payload_size > max_payload_size) {
        return -3;
    }
    
    // Primește payload dacă există
    if (header->payload_size > 0) {
        received = recv(socket, payload, header->payload_size, MSG_WAITALL);
        if (received != header->payload_size) {
            return -4;
        }
        
        // Verifică checksum
        uint32_t computed_checksum = calculate_checksum(payload, header->payload_size);
        if (computed_checksum != header->checksum) {
            return -5;
        }
    }
    
    return 0;
}

// Exemple de utilizare pentru diferite scenarii
void example_agent_registration(int socket, const char* agent_id) {
    AgentRegistration reg = {0};
    strncpy(reg.agent_id, agent_id, sizeof(reg.agent_id) - 1);
    reg.capabilities_flags = 0x1234;  // Example flags
    reg.memory_available = 1024 * 1024 * 1024;  // 1GB
    
    send_message(socket, MSG_AGENT_REGISTER, &reg, sizeof(reg));
}

void example_submit_task(int socket, const char* command, const char* args) {
    TaskSubmission task = {0};
    strncpy(task.command, command, sizeof(task.command) - 1);
    strncpy(task.arguments, args, sizeof(task.arguments) - 1);
    task.requirements_flags = 0x5678;  // Example flags
    task.timeout = 30;  // 30 seconds
    
    send_message(socket, MSG_TASK_SUBMIT, &task, sizeof(task));
}

void example_send_result(int socket, uint32_t task_id, const char* result) {
    TaskResult res = {0};
    res.task_id = task_id;
    res.status_code = 0;  // Success
    strncpy(res.result, result, sizeof(res.result) - 1);
    
    send_message(socket, MSG_TASK_RESULT, &res, sizeof(res));
}

#endif