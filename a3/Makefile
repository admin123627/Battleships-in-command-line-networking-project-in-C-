# CSC209 Battleship Socket Project Makefile
# Compile client: make all
# Compile server: make server
# Build unit tests: make test_game
# Build integration tests: make integration-tests
# Clean: make clean

# Compiler and compilation flags
CC = cc
CFLAGS = -Wall -Wextra -g

# Object files shared between client and server
COMMON_OBJS = utils.o game.o socket.o

# Default target: build client, server, and unit tests
all: client server test_game

# Client executable
client: client.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o client client.o $(COMMON_OBJS)

# Server executable
server: server.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o server server.o $(COMMON_OBJS)

# Unit tests for game logic
test_game: test_game.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o test_game test_game.o $(COMMON_OBJS)

# Integration tests (requires tests/ directory)
integration-tests:
	cd tests && $(MAKE)

# Generic rule: compile any .c file to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Clean up all compiled files and executables
clean:
	rm -f *.o client server test_game
	cd tests && $(MAKE) clean

# Declare these targets as phony (not actual files on disk)
.PHONY: all clean server integration-tests

