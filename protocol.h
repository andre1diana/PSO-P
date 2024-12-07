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
uint32_t calculate_checksum(const void* data, size_t size);

// Functie pentru trimiterea unui mesaj complet
int send_message(int socket, MessageType type, const void* payload, size_t payload_size);

// Functie pentru primirea unui mesaj complet
int receive_message(int socket, MessageHeader* header, void* payload, size_t max_payload_size);


#endif