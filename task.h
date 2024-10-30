#include <stdint.h>

//tasks structure

//id for task
//client that req the task
//agent assigned for task
//status
//result ?  -- in loc sa tinem agentul blocat pana cere clientul rezultatul taskului
//          -- nu mai avem nevoie de return_ready

typedef struct TASK_DATA{
    uint16_t ID_task;
    uint16_t ID_client;
    uint16_t ID_agent;
    uint32_t executable_size;
    uint32_t data_size;
    uint32_t executable_offset;
    uint32_t data_offset;
    uint8_t task_type;  // 0 - executable
                        // 1 - bash command
}TASK_DATA;

struct message;

massage.taskdata =...
message.exe = 