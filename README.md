# Command-Line Battleship

## Overview

This project is a command-line multiplayer Battleship game written in C for the CSC209 Systems Programming Mini-Project. This project was done in a group of 2, just me and my partner. The project follows the **client-server socket application** category, where one server process manages multiple connected clients using TCP sockets. The server is responsible for maintaining the authoritative game state, assigning players to rooms, validating moves, and coordinating gameplay between two players.

The project was designed to demonstrate systems programming concepts such as socket programming, file descriptors, structured message passing, and concurrent client handling using a server event loop. 

## Features

- Two-player Battleship gameplay through the terminal
- TCP client-server architecture
- Multiple game rooms
- Room creation and joining
- Server-assigned room IDs and player IDs
- Board setup and validation
- Turn-based shooting system
- Hit, miss, and sunk-ship handling
- Server-side authoritative game state
- Fixed-size message protocol between client and server
- Graceful handling of client disconnects where possible

## My Contributions

I worked primarily on the **server-side implementation** of the project. My work focused on building the networking and game-coordination logic that allows multiple clients to connect and play Battleship through the server.

My contributions included:

- Implementing the main server loop
- Managing connected clients
- Creating and tracking game rooms
- Assigning `room_id` and `player_id` values
- Handling client requests such as creating rooms, joining rooms, submitting boards, and making shots
- Validating submitted boards using the shared game logic
- Maintaining the authoritative game state on the server
- Coordinating turns between players
- Sending game updates, error messages, and status messages back to clients
- Handling client disconnections and cleaning up server-side resources
- Keeping the server compatible with the existing shared `Message` protocol and client implementation

My partner worked primarily on the **client-side implementation**. Their work focused on the user-facing terminal interface and communication with the server.

## Project Structure

```text
.
├── client.c        # Client-side logic and user interaction
├── client.h        # Client-side function declarations
├── server.c        # Server-side networking, rooms, and game coordination
├── server.h        # Server-side structs and function declarations
├── game.c          # Core Battleship game logic
├── game.h          # Game structs and game logic declarations
├── protocol.h      # Shared message types and Message struct
├── socket.c        # Socket helper functions
├── socket.h        # Socket helper declarations
├── utils.c         # Shared send/receive helper functions
├── utils.h         # Utility function declarations
├── Makefile        # Builds the project
└── README.md       # Project documentation
