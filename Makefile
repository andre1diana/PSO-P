# Compiler
CC = gcc

# Flags
CFLAGS = -Wall -Wextra -g

# Targets
TARGETS = server agent

# Source files
SERVER_SOURCES = server.c protocol.c
AGENT_SOURCES = agent.c protocol.c

# Header files
HEADERS = protocol.h common.h

# Build executables
all: $(TARGETS)

server: $(SERVER_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o server $(SERVER_SOURCES)

agent: $(AGENT_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o agent $(AGENT_SOURCES)

# Run targets
run_server: server
	./server

run_agent: agent
	./agent

# Cleanup generated files
clean:
	rm -f $(TARGETS)
