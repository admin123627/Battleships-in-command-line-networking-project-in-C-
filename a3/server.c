#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "server.h"
#include "socket.h"

/*
 * main - Entry point for the Battleship server.
 *
 * Sets up the server to listen for incoming client connections on PORT.
 * Initializes the clients and rooms arrays, then starts the main event loop.
 *
 * Returns:
 *   0 on normal exit
 *   1 on error (socket setup failed, etc.)
 */
int main(void) {
    printf("Starting Battleship server on port %d...\n", PORT);

    /* Initialize server address structure */
    struct sockaddr_in *server_addr = init_server_addr(PORT);
    if (server_addr == NULL) {
        fprintf(stderr, "Failed to initialize server address\n");
        return 1;
    }

    /* Set up listening socket */
    int listen_fd = set_up_server_socket(server_addr, MAX_CLIENTS);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to set up server socket\n");
        return 1;
    }

    /* Initialize clients array - mark all as inactive */
    Client clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].is_connected = 0;
        clients[i].socket_fd = -1;
        clients[i].assigned_room_id = -1;
        clients[i].assigned_player_id = -1;
        memset(clients[i].name, 0, MAX_NAME_LEN);
    }

    /* Initialize rooms array - mark all as inactive */
    Room rooms[MAX_ROOMS];
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].is_active = 0;
        rooms[i].room_id = -1;
        rooms[i].current_player_count = 0;
        rooms[i].is_game_started = 0;
        rooms[i].player_boards_submitted[0] = 0;
        rooms[i].player_boards_submitted[1] = 0;
        rooms[i].connected_players[0] = NULL;
        rooms[i].connected_players[1] = NULL;
    }

    printf("Server initialized. Waiting for clients...\n");

    /* Start main event loop */
    run_server(listen_fd, clients, rooms);

    /* Clean up */
    close(listen_fd);
    printf("Server shut down.\n");

    return 0;
}

/*
 * run_server - Main event loop using select().
 *
 * Monitors the listening socket and all active client sockets simultaneously.
 * When listen_fd is readable, accepts a new client connection.
 * When a client socket is readable, receives and processes their message.
 * Handles client disconnections.
 *
 * Parameters:
 *   listen_fd - file descriptor of the listening socket
 *   clients - array of Client structures
 *   rooms - array of Room structures
 *
 * Returns:
 *   void (loops indefinitely until shutdown signal)
 */
void run_server(int listen_fd, Client clients[], Room rooms[]) {
    while (1) {
        /* Set up fd_set for select() monitoring */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);

        /* Find the highest file descriptor number */
        int max_fd = listen_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].is_connected) {
                FD_SET(clients[i].socket_fd, &readfds);
                if (clients[i].socket_fd > max_fd) {
                    max_fd = clients[i].socket_fd;
                }
            }
        }

        /* Wait for activity on any socket (no timeout - blocks indefinitely) */
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select");
            exit(1);
        }

        /* Check if listen_fd has a new incoming connection */
        if (FD_ISSET(listen_fd, &readfds)) {
            handle_new_connection(listen_fd, clients);
        }

        /* Check each active client for incoming messages */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].is_connected) {
                continue;  /* This slot is not in use */
            }

            if (FD_ISSET(clients[i].socket_fd, &readfds)) {
                /* This client has data to read */
                handle_client_message(clients, rooms, i);
            }
        }
    }
}

/*
 * handle_new_connection - Accept a new client connection and add to clients array.
 *
 * Accepts the incoming connection from listen_fd and stores it in the first
 * available slot in the clients array. If the server is full, rejects the connection.
 *
 * Parameters:
 *   listen_fd - file descriptor of the listening socket
 *   clients - array of Client structures
 *
 * Returns: void
 */
void handle_new_connection(int listen_fd, Client clients[]) {
    /* Accept the incoming connection */
    int client_fd = accept_connection(listen_fd);
    if (client_fd < 0) {
        fprintf(stderr, "Failed to accept connection\n");
        return;
    }

    /* Find the first available slot in clients array */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected == 0) {
            /* Slot is available - add new client */
            clients[i].socket_fd = client_fd;
            clients[i].is_connected = 1;
            clients[i].assigned_room_id = -1;
            clients[i].assigned_player_id = -1;
            memset(clients[i].name, 0, MAX_NAME_LEN);

            printf("Client connected on socket %d (slot %d)\n", client_fd, i);
            return;
        }
    }

    /* No available slot - server is full */
    close(client_fd);
    printf("Server full - rejected connection on socket %d\n", client_fd);
}

/*
 * handle_client_message - Receive and dispatch client messages.
 *
 * Receives a message from the client and routes it to the appropriate handler
 * based on message type. Handles room creation/joining, board submission,
 * and game play messages.
 *
 * Parameters:
 *   clients - array of Client structures
 *   rooms - array of Room structures
 *   client_index - index in clients array for this client
 *
 * Returns:
 *   void
 */
void handle_client_message(Client clients[], Room rooms[], int client_index) {
    Client *client = &clients[client_index];
    Message msg;

    /* Receive the message from the client */
    if (receive_message(client->socket_fd, &msg) < 0) {
        printf("Client %d disconnected (recv failed)\n", client_index);
        remove_client(clients, rooms, client_index);
        return;
    }

    /* Route based on message type */
    switch (msg.type) {
        case MSG_CREATE_ROOM: {
            printf("Client %d requesting to create room\n", client_index);
            
            /* Create a new room */
            int room_id = create_room(rooms);
            if (room_id < 0) {
                /* Server is full */
                send_error(client->socket_fd, "Server full - no rooms available");
                break;
            }

            /* Join the client to the new room */
            int player_id = join_room(rooms, client, room_id);
            if (player_id < 0) {
                /* This shouldn't happen if create_room succeeded, but check anyway */
                send_error(client->socket_fd, "Failed to join new room");
                break;
            }

            /* Send MSG_ROOM_CREATED response */
            Message response;
            memset(&response, 0, sizeof(Message));
            response.type = MSG_ROOM_CREATED;
            response.room_id = room_id;
            response.player_id = player_id;

            if (send_message(client->socket_fd, &response) < 0) {
                printf("Failed to send MSG_ROOM_CREATED to client %d\n", client_index);
                remove_client(clients, rooms, client_index);
                break;
            }

            printf("Room %d created with client %d as player %d\n", room_id, client_index, player_id);
            break;
        }

        case MSG_JOIN_ROOM: {
            printf("Client %d requesting to join room %d\n", client_index, msg.room_id);
            
            /* Try to join the specified room */
            int player_id = join_room(rooms, client, msg.room_id);
            if (player_id < 0) {
                /* Determine why join failed and send appropriate error */
                Room *room = find_room(rooms, msg.room_id);
                if (room == NULL) {
                    send_error(client->socket_fd, "Room not found");
                } else {
                    send_error(client->socket_fd, "Room is full");
                }
                break;
            }

            /* Send MSG_JOIN_OK response */
            Message response;
            memset(&response, 0, sizeof(Message));
            response.type = MSG_JOIN_OK;
            response.room_id = msg.room_id;
            response.player_id = player_id;

            if (send_message(client->socket_fd, &response) < 0) {
                printf("Failed to send MSG_JOIN_OK to client %d\n", client_index);
                remove_client(clients, rooms, client_index);
                break;
            }

            /* Send MSG_OPPONENT_JOINED to the new joiner */
            Message waiting;
            memset(&waiting, 0, sizeof(Message));
            waiting.type = MSG_OPPONENT_JOINED;
            waiting.room_id = msg.room_id;
            waiting.player_id = player_id;
            if (send_message(client->socket_fd, &waiting) < 0) {
                printf("Failed to send MSG_OPPONENT_JOINED to joining player\n");
            }

            /* Notify the other player that opponent has joined */
            Room *room = find_room(rooms, msg.room_id);
            if (room != NULL && room->connected_players[1 - player_id] != NULL) {
                Message opp_waiting;
                memset(&opp_waiting, 0, sizeof(Message));
                opp_waiting.type = MSG_OPPONENT_JOINED;
                opp_waiting.room_id = msg.room_id;
                opp_waiting.player_id = 1 - player_id;
                if (send_message(room->connected_players[1 - player_id]->socket_fd, &opp_waiting) < 0) {
                    printf("Failed to send MSG_OPPONENT_JOINED to existing player\n");
                }
            }

            printf("Client %d joined room %d as player %d\n", client_index, msg.room_id, player_id);
            break;
        }

        case MSG_SUBMIT_BOARD: {
            printf("Client %d submitted board\n", client_index);
            
            /* Find the room this client is in */
            Room *room = find_room(rooms, client->assigned_room_id);
            if (room == NULL) {
                printf("Error: Client %d not in a valid room\n", client_index);
                send_error(client->socket_fd, "Not in a valid room");
                break;
            }

            /* Validate client state - must be waiting for opponent */
            if (client->assigned_player_id < 0 || client->assigned_player_id >= ROOM_SIZE) {
                printf("Error: Client %d has invalid player_id %d\n", client_index, client->assigned_player_id);
                send_error(client->socket_fd, "Invalid player ID");
                break;
            }

            /* Validate and place all ships on the board */
            Board *player_board = &room->game.boards[client->assigned_player_id];
            if (validate_and_place_board_ships(player_board, msg.ships, client->socket_fd) == 0) {
                break;
            }

            /* Mark that this player has submitted their board */
            room->player_boards_submitted[client->assigned_player_id] = 1;
            printf("[SERVER] Player %d in room %d submitted board\n", 
                   client->assigned_player_id, room->room_id);

            /* Send MSG_BOARD_OK response */
            Message response;
            memset(&response, 0, sizeof(Message));
            response.type = MSG_BOARD_OK;
            response.room_id = room->room_id;
            response.player_id = client->assigned_player_id;

            if (send_message(client->socket_fd, &response) < 0) {
                printf("Failed to send MSG_BOARD_OK to client %d\n", client_index);
                remove_client(clients, rooms, client_index);
                break;
            }

            printf("Sent MSG_BOARD_OK to player %d\n", client->assigned_player_id);

            /* Check if both players have submitted their boards */
            if (room->player_boards_submitted[0] == 1 && room->player_boards_submitted[1] == 1) {
                start_game_when_ready(room);
            }

            break;
        }

        case MSG_SHOOT: {
            printf("Client %d sent shot at (%d, %d)\n", client_index, msg.x, msg.y);

            /* Find the room this client is in */
            Room *room = find_room(rooms, client->assigned_room_id);
            if (room == NULL) {
                printf("Error: Client %d not in a valid room\n", client_index);
                send_error(client->socket_fd, "Not in a valid room");
                break;
            }

            /* Verify game has actually started */
            if (room->is_game_started != 1) {
                printf("[SERVER] Player %d tried to shoot but game hasn't started\n", client_index);
                send_error(client->socket_fd, "Game hasn't started yet");
                break;
            }

            /* Verify it's this player's turn */
            if (room->game.current_turn != client->assigned_player_id) {
                printf("[SERVER] Player %d tried to shoot but it's player %d's turn\n", 
                       client->assigned_player_id, room->game.current_turn);
                send_error(client->socket_fd, "Not your turn");
                break;
            }

            /* Validate shot coordinates */
            if (!in_bounds(msg.x, msg.y)) {
                printf("[SERVER] Invalid coordinates (%d, %d) from player %d\n", 
                       msg.x, msg.y, client->assigned_player_id);
                send_error(client->socket_fd, "Coordinates out of bounds (0-9)");
                break;
            }

            /* Get opponent ID and their board */
            int opponent_id = 1 - client->assigned_player_id;
            Board *opponent_board = &room->game.boards[opponent_id];

            /* Process the shot */
            ShotResult result = take_shot(opponent_board, msg.x, msg.y);
            printf("[SERVER] Player %d shot at (%d, %d): %d\n", 
                   client->assigned_player_id, msg.x, msg.y, result);

            /* Send MSG_SHOT_RESULT back to shooter */
            Message response;
            memset(&response, 0, sizeof(Message));
            response.type = MSG_SHOT_RESULT;
            response.room_id = room->room_id;
            response.player_id = client->assigned_player_id;
            response.x = msg.x;
            response.y = msg.y;
            response.shot_result = result;

            if (send_message(client->socket_fd, &response) < 0) {
                printf("Failed to send MSG_SHOT_RESULT to player %d\n", client->assigned_player_id);
                remove_client(clients, rooms, client_index);
                break;
            }

            /* Send MSG_INCOMING_SHOT to opponent */
            Message incoming;
            memset(&incoming, 0, sizeof(Message));
            incoming.type = MSG_INCOMING_SHOT;
            incoming.room_id = room->room_id;
            incoming.player_id = opponent_id;
            incoming.x = msg.x;
            incoming.y = msg.y;
            incoming.shot_result = result;

            if (send_message(room->connected_players[opponent_id]->socket_fd, &incoming) < 0) {
                printf("Failed to send MSG_INCOMING_SHOT to player %d\n", opponent_id);
            }

            /* Check if game is over (opponent lost) */
            if (all_ships_sunk(opponent_board)) {
                end_game_and_disconnect(clients, rooms, room, client->assigned_player_id, client_index);
            } else {
                /* Game continues - switch turns */
                room->game.current_turn = 1 - room->game.current_turn;
                printf("[SERVER] Turn switched to player %d in room %d\n", 
                       room->game.current_turn, room->room_id);

                /* Send turn messages */
                send_turn_messages(room, room->game.current_turn);
            }

            break;
        }

        case MSG_DISCONNECT:
            printf("Client %d disconnecting\n", client_index);
            remove_client(clients, rooms, client_index);
            break;

        default:
            printf("Client %d sent unknown message type %d\n", client_index, msg.type);
            break;
    }
}

/*
 * create_room - Create a new game room.
 *
 * Finds the first inactive room slot and initializes it with a unique room_id.
 *
 * Parameters:
 *   rooms - array of Room structures
 *
 * Returns:
 *   room_id on success (positive integer)
 *   -1 on failure (no available rooms)
 */
int create_room(Room rooms[]) {
    /* Find first inactive room */
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].is_active == 0) {
            /* Found empty slot - initialize room */
            rooms[i].is_active = 1;
            rooms[i].room_id = i + 1000;  /* Simple room ID: 1000, 1001, etc. */
            rooms[i].current_player_count = 0;
            rooms[i].is_game_started = 0;
            rooms[i].player_boards_submitted[0] = 0;
            rooms[i].player_boards_submitted[1] = 0;
            rooms[i].connected_players[0] = NULL;
            rooms[i].connected_players[1] = NULL;

            /* Initialize game state for this room */
            init_game(&rooms[i].game);

            printf("[SERVER] Created room %d\n", rooms[i].room_id);
            return rooms[i].room_id;
        }
    }

    printf("[SERVER] No available rooms (server full)\n");
    return -1;
}

/*
 * find_room - Look up a room by room_id.
 *
 * Searches through rooms array to find a room with matching room_id.
 *
 * Parameters:
 *   rooms - array of Room structures
 *   room_id - room ID to search for
 *
 * Returns:
 *   Pointer to Room on success
 *   NULL if not found
 */
Room *find_room(Room rooms[], int room_id) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].is_active && rooms[i].room_id == room_id) {
            return &rooms[i];
        }
    }
    return NULL;
}

/*
 * join_room - Add a client to a room.
 *
 * Finds the specified room and adds the client to it.
 * Assigns player_id based on current_player_count (0 for first, 1 for second).
 * Prints debug messages to server console only.
 *
 * Parameters:
 *   rooms - array of Room structures
 *   client - pointer to Client to join
 *   room_id - room ID to join
 *
 * Returns:
 *   player_id on success (0 or 1)
 *   -1 on failure (room not found or full)
 */
int join_room(Room rooms[], Client *client, int room_id) {
    /* Find the room */
    Room *room = find_room(rooms, room_id);
    if (room == NULL) {
        printf("[SERVER] Room %d not found\n", room_id);
        return -1;
    }

    /* Check if room has space */
    if (room->current_player_count >= ROOM_SIZE) {
        printf("[SERVER] Room %d is full\n", room_id);
        return -1;
    }

    /* Assign player_id based on current count */
    int player_id = room->current_player_count;

    /* Add client to room */
    room->connected_players[player_id] = client;
    client->assigned_player_id = player_id;
    client->assigned_room_id = room_id;

    /* Increment player count */
    room->current_player_count++;

    printf("[SERVER] Client added to room %d as player %d\n", room_id, player_id);
    return player_id;
}

/*
 * validate_and_place_board_ships - Validate and place all ships from a board submission.
 *
 * Converts ShipPlacement structures to Ship structures, validates placement on the board,
 * and places them. If any ship fails validation, sends error to client and returns 0.
 *
 * Parameters:
 *   board - pointer to the Board to place ships on
 *   ships - array of ShipPlacement structures from client message
 *   socket_fd - client socket to send error to if validation fails
 *
 * Returns:
 *   1 on success (all ships placed)
 *   0 on failure (any ship placement failed)
 */
int validate_and_place_board_ships(Board *board, ShipPlacement ships[], int socket_fd) {
    for (int i = 0; i < MAX_SHIPS; i++) {
        ShipPlacement placement = ships[i];
        
        /* Create Ship from ShipPlacement */
        Ship ship;
        ship.x = placement.x;
        ship.y = placement.y;
        ship.length = placement.length;
        ship.orientation = placement.orientation;
        ship.hits = 0;
        ship.sunk = 0;
        
        /* Place ship on the board */
        if (place_ship(board, ship) < 0) {
            printf("Error: Failed to place ship %d\n", i);
            send_error(socket_fd, "Board validation failed - invalid ship placement");
            return 0;
        }
    }

    return 1;
}

/*
 * start_game_when_ready - Start the game when both players have submitted boards.
 *
 * Sends MSG_GAME_START to both players, initializes turn (player 0 first),
 * and sends initial turn messages to both players.
 *
 * Parameters:
 *   room - pointer to the Room to start
 *
 * Returns:
 *   void
 */
void start_game_when_ready(Room *room) {
    printf("[SERVER] Both players in room %d have submitted boards! Starting game.\n", 
           room->room_id);

    /* Ensure both players are still connected before sending game start */
    if (room->connected_players[0] == NULL || room->connected_players[1] == NULL) {
        printf("[SERVER] Error: A player disconnected before game start\n");
        return;
    }

    /* Send MSG_GAME_START to both players */
    Message game_start;
    memset(&game_start, 0, sizeof(Message));
    game_start.type = MSG_GAME_START;
    game_start.room_id = room->room_id;

    /* Send to player 0 */
    game_start.player_id = 0;
    if (send_message(room->connected_players[0]->socket_fd, &game_start) < 0) {
        printf("Failed to send MSG_GAME_START to player 0\n");
    }

    /* Send to player 1 */
    game_start.player_id = 1;
    if (send_message(room->connected_players[1]->socket_fd, &game_start) < 0) {
        printf("Failed to send MSG_GAME_START to player 1\n");
    }

    /* Mark game as started and initialize turn */
    room->is_game_started = 1;
    room->game.current_turn = 0;
    printf("[SERVER] Game started in room %d. Player 0's turn.\n", room->room_id);

    /* Send initial turn messages */
    send_turn_messages(room, 0);
}

/*
 * end_game_and_disconnect - End the game, notify players, and disconnect both.
 *
 * Sends MSG_GAME_OVER to both players with the winner_id, then disconnects
 * both clients and cleans up the room.
 *
 * Parameters:
 *   clients - array of Client structures
 *   rooms - array of Room structures
 *   room - pointer to the Room where game ended
 *   winner_player_id - player ID (0 or 1) who won
 *   client_index - index of the client who sent the winning shot
 *
 * Returns:
 *   void
 */
void end_game_and_disconnect(Client clients[], Room rooms[], Room *room, 
                              int winner_player_id, int client_index) {
    int opponent_id = 1 - winner_player_id;

    printf("[SERVER] All ships sunk! Player %d wins in room %d!\n", 
           winner_player_id, room->room_id);

    /* Send MSG_GAME_OVER to both players */
    Message game_over;
    memset(&game_over, 0, sizeof(Message));
    game_over.type = MSG_GAME_OVER;
    game_over.room_id = room->room_id;
    game_over.winner_id = winner_player_id;

    /* Send to winner */
    if (send_message(clients[client_index].socket_fd, &game_over) < 0) {
        printf("Failed to send MSG_GAME_OVER to winner\n");
    }

    /* Send to loser */
    if (room->connected_players[opponent_id] != NULL) {
        if (send_message(room->connected_players[opponent_id]->socket_fd, &game_over) < 0) {
            printf("Failed to send MSG_GAME_OVER to loser\n");
        }
    }

    /* Mark game as over */
    room->is_game_started = 0;

    /* Disconnect both players and clean up room */
    printf("[SERVER] Cleaning up game in room %d\n", room->room_id);
    
    /* Find and remove opponent by searching clients array */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].is_connected && 
            clients[i].assigned_room_id == room->room_id &&
            clients[i].assigned_player_id == opponent_id) {
            remove_client(clients, rooms, i);
            break;
        }
    }
    
    /* Remove current player (caller) */
    remove_client(clients, rooms, client_index);
}

/*
 * send_turn_messages - Send turn notification messages to both players.
 *
 * Tells the current player it's their turn, and tells the other player to wait.
 * Called after initialization and after each shot to transition turns.
 *
 * Parameters:
 *   room - pointer to the Room
 *   current_player_id - player ID who's turn it is (0 or 1)
 *
 * Returns:
 *   void
 */
void send_turn_messages(Room *room, int current_player_id) {
    int opponent_id = 1 - current_player_id;

    /* Verify both players are still connected */
    if (room->connected_players[current_player_id] == NULL || 
        room->connected_players[opponent_id] == NULL) {
        printf("[SERVER] Warning: Cannot send turn messages - a player disconnected\n");
        return;
    }

    /* Send MSG_YOUR_TURN to current player */
    Message your_turn;
    memset(&your_turn, 0, sizeof(Message));
    your_turn.type = MSG_YOUR_TURN;
    your_turn.room_id = room->room_id;
    your_turn.player_id = current_player_id;

    if (send_message(room->connected_players[current_player_id]->socket_fd, &your_turn) < 0) {
        printf("[SERVER] Failed to send MSG_YOUR_TURN to player %d\n", current_player_id);
    }

    /* Send MSG_WAIT_TURN to opponent */
    Message wait_turn;
    memset(&wait_turn, 0, sizeof(Message));
    wait_turn.type = MSG_WAIT_TURN;
    wait_turn.room_id = room->room_id;
    wait_turn.player_id = opponent_id;

    if (send_message(room->connected_players[opponent_id]->socket_fd, &wait_turn) < 0) {
        printf("[SERVER] Failed to send MSG_WAIT_TURN to player %d\n", opponent_id);
    }
}

/*
 * send_error - Send an error message to a client.
 *
 * Creates and sends a MSG_ERROR message with the provided error text.
 *
 * Parameters:
 *   fd - socket file descriptor to send to
 *   msg - error message text
 *
 * Returns:
 *   0 on success
 *   -1 on failure (send error)
 */
int send_error(int fd, const char *msg) {
    Message error_msg;
    memset(&error_msg, 0, sizeof(Message));
    error_msg.type = MSG_ERROR;
    strncpy(error_msg.error_msg, msg, MAX_ERROR_LEN - 1);

    if (send_message(fd, &error_msg) < 0) {
        fprintf(stderr, "[SERVER] Failed to send error message\n");
        return -1;
    }

    return 0;
}

/*
 * remove_client - Remove a client and clean up resources.
 *
 * Closes the client's socket, removes them from any room, and marks the slot as inactive.
 * Will yuse when game ends.
 * 
 * Parameters:
 *   clients - array of Client structures
 *   rooms - array of Room structures
 *   client_index - index in clients array
 *
 * Returns:
 *   void
 */
void remove_client(Client clients[], Room rooms[], int client_index) {
    Client *client = &clients[client_index];

    /* If client was in a room, remove them from it */
    if (client->assigned_room_id >= 0) {
        Room *room = find_room(rooms, client->assigned_room_id);
        if (room != NULL) {
            /* Find and remove this client from the room's players */
            for (int i = 0; i < ROOM_SIZE; i++) {
                if (room->connected_players[i] == client) {
                    room->connected_players[i] = NULL;
                    room->current_player_count--;
                    break;
                }
            }

            /* If room is now empty, deactivate it */
            if (room->current_player_count == 0) {
                room->is_active = 0;
                printf("[SERVER] Room %d deactivated (empty)\n", room->room_id);
            }
        }
    }

    /* Close the socket */
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }

    /* Mark slot as inactive */
    client->is_connected = 0;
    client->socket_fd = -1;
    client->assigned_room_id = -1;
    client->assigned_player_id = -1;

    printf("[SERVER] Client %d removed\n", client_index);
}

