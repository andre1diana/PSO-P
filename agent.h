//agent structure
typedef struct AGENT_DATA{
    int agent_ID; // identifier for agent
    int busy_flag; // value 1 if agent executes tasks and 0 if agent is free
    int return_ready_flag; // value 1 if agent is done executing the task and ready to return the result and 0 if agent is still executing
    int agent_type;/* agent types : 
                        1 - executes only on linux
                        2 - executes only simple tasks
                        3 - executes on ARM microcontroler (a list of tasks implemented on it)
                        ...
                    */
    
}AGENT_DATA;