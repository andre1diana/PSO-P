#include "protocol.h"
#include <sys/socket.h>

uint32_t calculate_checksum(const void* data, size_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t checksum = 0;
    
    for(size_t i = 0; i < size; i++) {
        checksum = (checksum << 8) | (checksum >> 24);
        checksum += bytes[i];
    }
    
    return checksum;
}

int send_message(int socket, MessageType type, const void* payload, size_t payload_size)
{
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