# Compiler
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -g 

# Targets
TARGETS = server agent client

# Source files
SERVER_SOURCES = server.c protocol.c
AGENT_SOURCES = agent.c protocol.c
CLIENT_SOURCES = client.c protocol.c

# Header files
HEADERS = protocol.h common.h

# Build executables
all: $(TARGETS)

server: $(SERVER_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o server $(SERVER_SOURCES)

agent: $(AGENT_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o agent $(AGENT_SOURCES)

client: $(CLIENT_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o client $(CLIENT_SOURCES)

# Run targets
run_server: server
	./server

run_agent: agent
	./agent

run_client: client
	./client

# Cleanup generated files
clean:
	rm -f $(TARGETS)
