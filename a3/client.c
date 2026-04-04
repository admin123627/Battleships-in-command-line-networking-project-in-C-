#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client.h"
#include "utils.h"
#include "socket.h"
#include "protocol.h"
#include "game.h"

/*
 * client_init - Initialize a Client struct with default values.
 *
 * Sets all fields to safe defaults:
 * - fd = -1 (not connected)
 * - room_id and player_id = -1 (not assigned by server)
 * - name is cleared
 * - state = CLIENT_INIT
 * - both boards are initialized
 */
void client_init(Client *client) {
    client->fd = -1;
    client->room_id = -1;
    client->player_id = -1;
    memset(client->name, 0, MAX_NAME_LEN);
    client->state = CLIENT_INIT;
    init_board(&client->own_board);
    init_board(&client->enemy_view);
}

/*
 * client_connect_to_server - Connect the client to the server.
 *
 * Calls socket.h's connect_to_server() to establish a TCP connection.
 * On success, stores the socket fd and sets state to CLIENT_CONNECTED.
 * On failure, sets state to CLIENT_ERROR.
 *
 * Returns:
 *   0 on success
 *   -1 on connection failure
 */
int client_connect_to_server(Client *client, int port, const char *hostname) {
    int fd = connect_to_server(port, hostname);
    if (fd < 0) {
        fprintf(stderr, "Error: Failed to connect to server at %s:%d\n", hostname, port);
        client->state = CLIENT_ERROR;
        return -1;
    }

    client->fd = fd;
    client->state = CLIENT_CONNECTED;
    printf("Connected to server at %s:%d\n", hostname, port);

    return 0;
}

/*
 * client_create_room - Send a create room request to the server.
 *
 * Flow:
 * 1. Prompt player for a name
 * 2. Send MSG_CREATE_ROOM with player name
 * 3. Receive MSG_ROOM_CREATED from server (contains room_id and player_id)
 * 4. Store room info and update state to CLIENT_WAITING_FOR_OPPONENT
 *
 * Returns:
 *   0 on success
 *   -1 on failure (send/recv error, wrong response type, etc.)
 */
int client_create_room(Client *client) {
    if (client->state != CLIENT_CONNECTED) {
        fprintf(stderr, "Error: Must be connected before creating a room.\n");
        return -1;
    }

    /* Prompt for player name */
    printf("Enter your player name: ");
    fgets(client->name, MAX_NAME_LEN - 1, stdin);

    /* Remove trailing newline from fgets */
    size_t len = strlen(client->name);
    if (len > 0 && client->name[len - 1] == '\n') {
        client->name[len - 1] = '\0';
    }

    /* Prepare and send MSG_CREATE_ROOM message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_CREATE_ROOM;
    strncpy(msg.name, client->name, MAX_NAME_LEN - 1);

    if (send_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Failed to send CREATE_ROOM message to server.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    /* Receive and parse MSG_ROOM_CREATED response */
    if (receive_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Failed to receive ROOM_CREATED response from server.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    if (msg.type != MSG_ROOM_CREATED) {
        fprintf(stderr, "Error: Expected MSG_ROOM_CREATED but received type %d\n", msg.type);
        client->state = CLIENT_ERROR;
        return -1;
    }

    /* Store room and player IDs from server response */
    client->room_id = msg.room_id;
    client->player_id = msg.player_id;
    client->state = CLIENT_WAITING_FOR_OPPONENT;

    printf("Room created successfully!\n");
    printf("  Room ID: %d\n", client->room_id);
    printf("  Your Player ID: %d\n", client->player_id);
    printf("Waiting for opponent to join...\n");

    return 0;
}

/*
 * client_join_room - Send a join room request to the server.
 *
 * Flow:
 * 1. Prompt player for name and room ID to join
 * 2. Send MSG_JOIN_ROOM with player name and room_id
 * 3. Receive MSG_JOIN_OK from server (contains room_id and player_id)
 * 4. Store room info and update state to CLIENT_WAITING_FOR_OPPONENT
 *
 * Returns:
 *   0 on success
 *   -1 on failure (send/recv error, wrong response type, room not found, etc.)
 */
int client_join_room(Client *client) {
    if (client->state != CLIENT_CONNECTED) {
        fprintf(stderr, "Error: Must be connected before joining a room.\n");
        return -1;
    }

    /* Prompt for player name */
    printf("Enter your player name: ");
    fgets(client->name, MAX_NAME_LEN - 1, stdin);

    /* Remove trailing newline from fgets */
    size_t len = strlen(client->name);
    if (len > 0 && client->name[len - 1] == '\n') {
        client->name[len - 1] = '\0';
    }

    /* Prompt for room ID */
    printf("Enter the room ID to join: ");
    int room_id;
    if (scanf("%d", &room_id) != 1) {
        fprintf(stderr, "Error: Invalid room ID input.\n");
        return -1;
    }
    getchar();  /* Consume the newline left by scanf */

    /* Prepare and send MSG_JOIN_ROOM message */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_JOIN_ROOM;
    msg.room_id = room_id;
    strncpy(msg.name, client->name, MAX_NAME_LEN - 1);

    if (send_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Failed to send JOIN_ROOM message to server.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    /* Receive and parse MSG_JOIN_OK or error response */
    if (receive_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Failed to receive JOIN_OK response from server.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    if (msg.type == MSG_ERROR) {
        fprintf(stderr, "Error: Server rejected join request.\n");
        if (msg.error_msg[0] != '\0') {
            fprintf(stderr, "  Reason: %s\n", msg.error_msg);
        }
        client->state = CLIENT_ERROR;
        return -1;
    }

    if (msg.type != MSG_JOIN_OK) {
        fprintf(stderr, "Error: Expected MSG_JOIN_OK but received type %d\n", msg.type);
        client->state = CLIENT_ERROR;
        return -1;
    }

    /* Store room and player IDs from server response */
    client->room_id = msg.room_id;
    client->player_id = msg.player_id;
    client->state = CLIENT_WAITING_FOR_OPPONENT;

    printf("Successfully joined room %d\n", client->room_id);
    printf("  Your Player ID: %d\n", client->player_id);
    printf("Waiting for opponent...\n");

    return 0;
}

/*
 * client_place_ships - Interactively place ships on the client's board.
 *
 * Flow:
 * 1. For each of the MAX_SHIPS ships:
 *    - Display ship name and length
 *    - Prompt for starting x/y coordinates (0-9)
 *    - Prompt for orientation (H for horizontal, V for vertical)
 *    - Validate placement using validate_ship_placement()
 *    - Place ship on own_board using place_ship()
 *    - Display updated board
 * 2. Set client state to CLIENT_BOARD_SETUP after all ships are placed
 *
 * Returns:
 *   0 on success (all ships placed)
 *   -1 on input error (EOF, etc.)
 */
int client_place_ships(Client *client) {
    printf("\n====== Board Setup ======\n");
    printf("You must place %d ships on your 10x10 board.\n\n", MAX_SHIPS);

    /* Ship specs: length and name for display */
    int ship_lengths[MAX_SHIPS] = {5, 4, 3, 3, 2};
    const char *ship_names[MAX_SHIPS] = {
        "Battleship",
        "Cruiser",
        "Destroyer",
        "Submarine",
        "Minesweeper"
    };

    /* Place each ship */
    for (int i = 0; i < MAX_SHIPS; i++) {
        printf("\nPlacing ship %d/%d: %s (length %d)\n",
               i + 1, MAX_SHIPS, ship_names[i], ship_lengths[i]);

        /* Keep prompting until valid placement is accepted */
        while (1) {
            /* Get starting coordinates */
            int x, y;
            if (get_player_input(&x, &y) < 0) {
                fprintf(stderr, "Error: Failed to read coordinates.\n");
                return -1;
            }

            /* Get and validate orientation - only H/h or V/v accepted */
            Orientation orientation;
            while (1) {
                printf("Orientation (H=horizontal, V=vertical): ");
                char orient_char;
                scanf(" %c", &orient_char);
                getchar();  /* Consume newline */

                if (orient_char == 'H' || orient_char == 'h') {
                    orientation = HORIZONTAL;
                    break;
                } else if (orient_char == 'V' || orient_char == 'v') {
                    orientation = VERTICAL;
                    break;
                } else {
                    printf("Invalid orientation. Please enter H or V.\n");
                }
            }

            /* Create and validate ship placement.
             * We validate orientation before creating ShipPlacement to ensure
             * the placement struct only contains valid data. */
            ShipPlacement placement;
            placement.x = x;
            placement.y = y;
            placement.length = ship_lengths[i];
            placement.orientation = orientation;

            /* Check if placement is valid */
            if (validate_ship_placement(&client->own_board, placement)) {
                /* Valid placement: create Ship and place it on board */
                Ship ship;
                ship.x = x;
                ship.y = y;
                ship.length = ship_lengths[i];
                ship.orientation = orientation;
                ship.hits = 0;
                ship.sunk = 0;

                place_ship(&client->own_board, ship);
                printf("Ship placed successfully!\n");
                break;
            } else {
                printf("Invalid placement (out of bounds or overlaps). Try again.\n");
            }
        }

        /* Show board after each ship placement */
        print_board(&client->own_board, 1);
    }

    client->state = CLIENT_BOARD_SETUP;
    printf("\nAll ships placed!\n");
    return 0;
}

/*
 * client_wait_for_opponent - Wait for the opponent to join the room.
 *
 * After creating or joining a room, this function waits for the server to confirm
 * that the opponent is now connected. This means both players are in the room and
 * can now begin board setup (placing ships).
 *
 * Flow:
 * 1. Display waiting message to user
 * 2. Loop waiting for messages from server:
 *    - If MSG_WAITING: Opponent not yet joined, continue waiting
 *    - If MSG_OPPONENT_JOINED: Both players are now in room, ready for board setup
 * 3. Update client state to CLIENT_BOARD_SETUP when opponent joins
 *
 * Protocol note:
 * - MSG_OPPONENT_JOINED signals that the room is full (both players connected)
 * - This is different from MSG_GAME_START, which comes later after both boards
 *   are submitted and validated.
 *
 * Returns:
 *   0 on success (opponent joined, both players in room)
 *   -1 on failure (connection lost, invalid message, etc.)
 */
int client_wait_for_opponent(Client *client) {
    if (client->state != CLIENT_WAITING_FOR_OPPONENT) {
        fprintf(stderr, "Error: Client must be in WAITING_FOR_OPPONENT state.\n");
        return -1;
    }

    printf("\nWaiting for opponent to join the room...\n");
    printf("(This may take a moment)\n\n");

    /* Loop until opponent joins and room is ready for board setup */
    Message msg;
    while (1) {
        if (receive_message(client->fd, &msg) < 0) {
            fprintf(stderr, "Error: Connection lost while waiting for opponent.\n");
            client->state = CLIENT_ERROR;
            return -1;
        }

        if (msg.type == MSG_WAITING) {
            /* Opponent not yet connected; keep waiting */
            printf("Waiting for opponent to join...\n");
            continue;
        } else if (msg.type == MSG_OPPONENT_JOINED) {
            /* Both players now in room; ready to place ships */
            printf("Opponent joined! Both players are now in the room.\n");
            printf("Time to set up your board!\n\n");
            client->state = CLIENT_BOARD_SETUP;
            return 0;
        } else {
            fprintf(stderr, 
                "Error: Unexpected message type %d while waiting for opponent.\n",
                msg.type);
            client->state = CLIENT_ERROR;
            return -1;
        }
    }
}

/*
 * client_submit_board - Send the client's board to the server.
 *
 * Flow:
 * 1. Create MSG_SUBMIT_BOARD with all ship placements from own_board
 * 2. Send message to server
 * 3. Receive MSG_BOARD_OK or MSG_BOARD_INVALID response
 * 4. If accepted, wait for MSG_GAME_START
 * 5. Update state to CLIENT_IN_GAME
 *
 * Returns:
 *   0 on success (board accepted and game started)
 *   -1 on failure (board rejected, message error, etc.)
 */
int client_submit_board(Client *client) {
    if (client->state != CLIENT_BOARD_SETUP) {
        fprintf(stderr, "Error: Board must be set up before submission.\n");
        return -1;
    }

    printf("\nSubmitting board to server...\n");

    /* Prepare MSG_SUBMIT_BOARD with ship placements */
    Message msg;
    memset(&msg, 0, sizeof(Message));
    msg.type = MSG_SUBMIT_BOARD;
    msg.room_id = client->room_id;
    msg.player_id = client->player_id;

    /* Copy ship placements from own_board into message */
    for (int i = 0; i < client->own_board.ships_placed; i++) {
        Ship *ship = &client->own_board.ships[i];
        msg.ships[i].x = ship->x;
        msg.ships[i].y = ship->y;
        msg.ships[i].length = ship->length;
        msg.ships[i].orientation = ship->orientation;
    }

    if (send_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Failed to send board to server.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    /* Wait for server validation response */
    if (receive_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Lost connection while waiting for board validation.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    if (msg.type == MSG_BOARD_INVALID) {
        fprintf(stderr, "Error: Server rejected your board setup.\n");
        if (msg.error_msg[0] != '\0') {
            fprintf(stderr, "  Reason: %s\n", msg.error_msg);
        }
        return -1;
    }

    if (msg.type != MSG_BOARD_OK) {
        fprintf(stderr, "Error: Unexpected response type %d\n", msg.type);
        return -1;
    }

    printf("Board accepted by server.\n");
    printf("Waiting for opponent's board and game start...\n");

    /* Wait for MSG_GAME_START */
    if (receive_message(client->fd, &msg) < 0) {
        fprintf(stderr, "Error: Lost connection waiting for game start.\n");
        client->state = CLIENT_ERROR;
        return -1;
    }

    if (msg.type != MSG_GAME_START) {
        fprintf(stderr, "Error: Expected MSG_GAME_START but got type %d\n", msg.type);
        return -1;
    }

    printf("\n*** Game Started! ***\n");
    client->state = CLIENT_IN_GAME;
    return 0;
}

/*
 * client_play_game - Main game loop for the client.
 *
 * Receives messages from the server and processes them:
 * - MSG_YOUR_TURN: Prompt player for shot coordinates and send MSG_SHOOT
 * - MSG_SHOT_RESULT: Update enemy_view and display the result (hit/miss/sunk)
 * - MSG_INCOMING_SHOT: Update own_board and display what opponent hit
 * - MSG_WAIT_TURN: Display waiting message and continue
 * - MSG_GAME_OVER: Determine and display the winner, exit loop
 * - MSG_ERROR: Display error message and return -1
 *
 * Returns:
 *   0 on normal game end
 *   -1 on error (connection lost, unexpected message, etc.)
 */
int client_play_game(Client *client) {
    if (client->state != CLIENT_IN_GAME) {
        fprintf(stderr, "Error: Game must be in progress.\n");
        return -1;
    }

    printf("\n=== Game Started! ===\n");
    printf("Let's play Battleship! Good luck!\n\n");

    Message msg;
    int game_running = 1;

    /* Main game loop: process messages from server until game ends */
    while (game_running) {
        /* Receive next message from server */
        if (receive_message(client->fd, &msg) < 0) {
            fprintf(stderr, "Error: Connection lost during gameplay.\n");
            client->state = CLIENT_ERROR;
            return -1;
        }

        /* Handle each message type from the server */
        switch (msg.type) {
            /* ===== OUR TURN: We shoot at the opponent ===== */
            case MSG_YOUR_TURN:
                printf("\n--- Your Turn! ---\n");
                printf("Current view of opponent's board:\n");
                print_board(&client->enemy_view, 0);

                /* Keep prompting until player selects a valid fresh cell (not already targeted) */
                int x, y;
                while (1) {
                    /* Prompt player for shot coordinates using helper function */
                    if (get_player_input(&x, &y) < 0) {
                        fprintf(stderr, "Error: Failed to read coordinates.\n");
                        return -1;
                    }

                    /* Check if this cell has already been targeted locally.
                     * Client-side validation prevents wasted server round-trips and gives
                     * the player immediate feedback if they pick a cell they've already shot at.
                     * This improves UX significantly. */
                    if (shot_already_taken(&client->enemy_view, x, y)) {
                        printf("You already fired at (%d, %d)! Choose a different cell.\n", x, y);
                        continue;  /* Prompt again */
                    }

                    /* Valid shot: cell has not been targeted yet, break out of validation loop */
                    break;
                }

                /* Create and send MSG_SHOOT message with validated coordinates */
                Message shoot_msg;
                memset(&shoot_msg, 0, sizeof(Message));
                shoot_msg.type = MSG_SHOOT;
                shoot_msg.x = x;
                shoot_msg.y = y;
                shoot_msg.player_id = client->player_id;
                shoot_msg.room_id = client->room_id;

                if (send_message(client->fd, &shoot_msg) < 0) {
                    fprintf(stderr, "Error: Failed to send shot to server.\n");
                    client->state = CLIENT_ERROR;
                    return -1;
                }

                printf("Shot sent to (%d, %d). Waiting for result...\n", x, y);
                break;

            /* ===== RESULT OF OUR SHOT ===== */
            case MSG_SHOT_RESULT:
                printf("\n--- Shot Result ---\n");

                /* Update enemy_view board to reflect the shot result */
                switch (msg.shot_result) {
                    case SHOT_MISS:
                        client->enemy_view.cells[msg.y][msg.x] = CELL_MISS;
                        printf("Your shot at (%d, %d) was a MISS!\n", msg.x, msg.y);
                        break;

                    case SHOT_HIT:
                        client->enemy_view.cells[msg.y][msg.x] = CELL_HIT;
                        printf("Your shot at (%d, %d) was a HIT!\n", msg.x, msg.y);
                        break;

                    case SHOT_SUNK:
                        client->enemy_view.cells[msg.y][msg.x] = CELL_HIT;
                        printf("Your shot at (%d, %d) SUNK an opponent's ship!\n", msg.x, msg.y);
                        break;

                    default:
                        printf("Unexpected shot result (%d).\n", msg.shot_result);
                        break;
                }
                break;

            /* ===== OPPONENT SHOT AT US ===== */
            case MSG_INCOMING_SHOT:
                printf("\n--- Opponent's Shot ---\n");

                /* Update own_board to reflect the opponent's shot result */
                switch (msg.shot_result) {
                    case SHOT_MISS:
                        client->own_board.cells[msg.y][msg.x] = CELL_MISS;
                        printf("Opponent shot at (%d, %d) and MISSED!\n", msg.x, msg.y);
                        break;

                    case SHOT_HIT:
                        client->own_board.cells[msg.y][msg.x] = CELL_HIT;
                        printf("Opponent shot at (%d, %d) and HIT your ship!\n", msg.x, msg.y);
                        break;

                    case SHOT_SUNK:
                        client->own_board.cells[msg.y][msg.x] = CELL_HIT;
                        printf("Opponent shot at (%d, %d) and SUNK one of your ships!\n", msg.x, msg.y);
                        break;

                    default:
                        printf("Unexpected shot result (%d).\n", msg.shot_result);
                        break;
                }
                break;

            /* ===== WAIT FOR OPPONENT'S TURN ===== */
            case MSG_WAIT_TURN:
                printf("Waiting for opponent to take their turn...\n");
                break;

            /* ===== GAME OVER ===== */
            case MSG_GAME_OVER:
                printf("\n");
                if (msg.winner_id == client->player_id) {
                    printf("*** YOU WON! ***\n");
                } else {
                    printf("*** YOU LOST! ***\n");
                }
                printf("Game has ended.\n");
                game_running = 0;  /* Exit the game loop */
                break;

            /* ===== ERROR FROM SERVER ===== */
            case MSG_ERROR:
                fprintf(stderr, "Server error: %s\n", msg.error_msg);
                client->state = CLIENT_ERROR;
                return -1;

            /* ===== UNEXPECTED MESSAGE TYPE ===== */
            default:
                printf("Received unexpected message type: %d\n", msg.type);
                break;
        }
    }

    /* Game loop ended normally */
    client->state = CLIENT_GAME_OVER;
    printf("\nThanks for playing Battleship!\n");
    return 0;
}

/*
 * client_disconnect - Close the connection and clean up the client.
 *
 * 1. Send MSG_DISCONNECT to the server (optional, connection close signals it)
 * 2. Close the socket
 * 3. Reset client state and IDs
 */
void client_disconnect(Client *client) {
    if (client->fd >= 0) {
        /* Send disconnect message to server */
        Message msg;
        memset(&msg, 0, sizeof(Message));
        msg.type = MSG_DISCONNECT;
        send_message(client->fd, &msg);

        /* Close socket */
        close(client->fd);
        client->fd = -1;
    }

    /* Reset client state */
    client->state = CLIENT_INIT;
    client->room_id = -1;
    client->player_id = -1;
}

/*
 * main - Entry point for the Battleship client.
 *
 * Usage: ./client <hostname> <port>
 *
 * Complete client lifecycle flow:
 * 1. Parse command-line arguments (hostname and port)
 * 2. Initialize Client struct (CLIENT_INIT state)
 * 3. Connect to the server (CLIENT_CONNECTED state)
 * 4. Create or join a room (CLIENT_WAITING_FOR_OPPONENT state)
 *    Server sets us in a room, waiting for second player
 * 5. Wait for opponent to join (receive MSG_OPPONENT_JOINED)
 *    When received, room is full and both players ready for setup
 * 6. Place ships on board (CLIENT_BOARD_SETUP state)
 * 7. Submit board to server (receive MSG_BOARD_OK)
 * 8. Wait for MSG_GAME_START (all boards submitted and validated)
 *    Now state becomes CLIENT_IN_GAME and actual gameplay begins
 * 9. Enter game play loop (CLIENT_IN_GAME state)
 * 10. Disconnect after game ends or on fatal error
 *
 * PROTOCOL PHASES:
 * - Room Setup Phase: Create/join room -> wait for MSG_OPPONENT_JOINED
 * - Board Setup Phase: Place ships locally -> submit board
 * - Gameplay Phase: Receive MSG_GAME_START -> play game -> MSG_GAME_OVER
 *
 * ERROR HANDLING:
 * - If any critical phase fails, disconnect and exit with error code 1
 * - Connection loss at any point triggers disconnection
 *
 * Returns:
 *   0 on normal exit (game ended normally)
 *   1 on error (invalid args, connection failure, fatal error)
 */
int main(int argc, char *argv[]) {
    /* ========== PHASE 1: PARSE ARGUMENTS AND INITIALIZE ========== */
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        fprintf(stderr, "Example: %s localhost 9999\n", argv[0]);
        return 1;
    }

    const char *hostname = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Port must be between 1 and 65535.\n");
        return 1;
    }

    /* Initialize the client (Client state: CLIENT_INIT) */
    Client client;
    client_init(&client);

    /* ========== PHASE 2: CONNECT TO SERVER ========== */
    printf("[PHASE 1] Initializing and connecting...\n");
    printf("Connecting to %s:%d...\n", hostname, port);
    if (client_connect_to_server(&client, port, hostname) < 0) {
        fprintf(stderr, "Failed to connect to server.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 3: CREATE OR JOIN ROOM ========== */
    printf("\n[PHASE 2] Room setup...\n");
    printf("Would you like to:\n");
    printf("  1) Create a new room\n");
    printf("  2) Join an existing room\n");
    printf("Enter your choice (1 or 2): ");

    int choice;
    scanf("%d", &choice);
    getchar();  /* Consume newline after input */

    if (choice == 1) {
        printf("\n--- Creating Room ---\n");
        if (client_create_room(&client) < 0) {
            fprintf(stderr, "Failed to create room.\n");
            client_disconnect(&client);
            return 1;
        }
    } else if (choice == 2) {
        printf("\n--- Joining Room ---\n");
        if (client_join_room(&client) < 0) {
            fprintf(stderr, "Failed to join room.\n");
            client_disconnect(&client);
            return 1;
        }
    } else {
        fprintf(stderr, "Invalid choice. Please enter 1 or 2.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 3: WAIT FOR OPPONENT (ROOM-READY PHASE) ========== */
    /* Wait for MSG_OPPONENT_JOINED: both players now in room, ready for board setup */
    printf("\n[PHASE 3] Waiting for opponent to join the room...\n");
    printf("(Board setup will begin once opponent joins)\n");
    if (client_wait_for_opponent(&client) < 0) {
        fprintf(stderr, "\nFailed while waiting for opponent.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 4: PLACE SHIPS (BOARD SETUP PHASE) ========== */
    printf("\n[PHASE 4] Placing ships on your board...\n");
    if (client_place_ships(&client) < 0) {
        fprintf(stderr, "\nFailed to place ships.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 5: SUBMIT BOARD (BOARD VALIDATION + GAME START) ========== */
    /* Submit board and wait for MSG_BOARD_OK, then MSG_GAME_START (both boards ready) */
    printf("\n[PHASE 5] Submitting board to server...\n");
    printf("Waiting for opponent's board and game start signal...\n");
    if (client_submit_board(&client) < 0) {
        fprintf(stderr, "\nFailed to submit board.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 6: PLAY GAME (GAMEPLAY PHASE) ========== */
    printf("\n[PHASE 6] Starting game play...\n");
    if (client_play_game(&client) < 0) {
        fprintf(stderr, "\nGame play ended with an error.\n");
        client_disconnect(&client);
        return 1;
    }

    /* ========== PHASE 7: DISCONNECT AFTER GAME ENDS ========== */
    printf("\n[PHASE 7] Game complete. Disconnecting...\n");
    client_disconnect(&client);
    printf("Client disconnected.\n");

    return 0;
}
