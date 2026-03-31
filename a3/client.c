#include <stdio.h>
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
 * TODO: Implement full game loop:
 * - MSG_YOUR_TURN: Prompt for shot, send MSG_SHOOT
 * - MSG_SHOT_RESULT: Receive result, update enemy_view
 * - MSG_INCOMING_SHOT: Receive shot, update own_board, send response
 * - MSG_WAIT_TURN: Wait for opponent's turn
 * - MSG_GAME_OVER: Receive result, determine winner
 *
 * Returns:
 *   0 on normal game end
 *   -1 on error
 */
int client_play_game(Client *client) {
    if (client->state != CLIENT_IN_GAME) {
        fprintf(stderr, "Error: Game must be in progress.\n");
        return -1;
    }

    printf("\n=== Game Loop ===\n");
    printf("TODO: Full game loop implementation not yet complete.\n");
    printf("Ready to play when server implementation is available.\n");

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
