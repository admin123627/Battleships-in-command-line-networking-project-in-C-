# CSC209 Battleship Socket Project Makefile

# Compiler and flags
CC = cc
CFLAGS = -Wall -Wextra -g

# Object files shared between client and server
COMMON_OBJS = utils.o game.o socket.o

# Build targets
all: client server

client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o client client.o $(COMMON_OBJS)

server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o server server.o $(COMMON_OBJS)

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Clean up build artifacts
clean:
	rm -f *.o client server

# Phony targets (not actual files)
.PHONY: all clean

