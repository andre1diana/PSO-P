#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Dimensiuni maxime pentru diferite campuri
#define MAX_PAYLOAD_SIZE 4096
#define MAX_COMMAND_SIZE 256
#define MAX_ARGS_SIZE 1024

// Tipuri de mesaje pentru identificarea continutului
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
    uint32_t sequence;      // Număr de secventa pentru tracking
    uint32_t payload_size;  // Dimensiune payload
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

// Functii pentru serializare/deserializare
uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    
    for(size_t i = 0; i < size; i++) {
        checksum = (checksum << 8) | (checksum >> 24);
        checksum += bytes[i];
    }
    
    return checksum;
}

// Functie pentru trimiterea unui mesaj complet
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
    
    // Trimite payload dacă exista
    if (payload_size > 0 && payload != NULL) {
        if (send(socket, payload, payload_size, MSG_NOSIGNAL) != payload_size) {
            return -1;
        }
    }
    
    return 0;
}

// Functie pentru primirea unui mesaj complet
int receive_message(int socket, MessageHeader* header, void* payload, size_t max_payload_size) {
    ssize_t received = recv(socket, header, sizeof(MessageHeader), MSG_WAITALL);
    if (received != sizeof(MessageHeader)) {
        return -1;
    }
    
    if (header->magic != 0xDEADBEEF) {
        return -2;
    }
    
    if (header->payload_size > max_payload_size) {
        return -3;
    }
    
    if (header->payload_size > 0) {
        received = recv(socket, payload, header->payload_size, MSG_WAITALL);
        if (received != header->payload_size) {
            return -4;
        }
        
        uint32_t computed_checksum = calculate_checksum(payload, header->payload_size);
        if (computed_checksum != header->checksum) {
            return -5;
        }
    }
    
    return 0;
}


#endif