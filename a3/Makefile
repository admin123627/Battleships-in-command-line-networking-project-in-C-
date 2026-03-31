# CSC209 Battleship Socket Project Makefile
# Compile client: make all
# Compile server: make server (currently incomplete - no main() yet)
# Clean: make clean

# Compiler and compilation flags
CC = cc
CFLAGS = -Wall -Wextra -g

# Object files shared between client and server
COMMON_OBJS = utils.o game.o socket.o

# Default target: build client only (server.c not yet complete)
all: client

# Client executable
client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o client client.o $(COMMON_OBJS)

# Server executable (currently optional - server.c needs main())
server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o server server.o $(COMMON_OBJS)

# Generic rule: compile any .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Clean up all compiled files and executables
clean:
	rm -f *.o client server

# Declare these targets as phony (not actual files on disk)
.PHONY: all clean server

