#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "game.h"

/*
 * ClientState enum - Represents the lifecycle phases of the client.
 *
 * CLIENT_INIT:              Client struct created but not connected
 * CLIENT_CONNECTED:         Connected to server, no room assigned yet
 * CLIENT_WAITING_FOR_OPPONENT: In a room, waiting for second player
 * CLIENT_BOARD_SETUP:       Setting up board (placing ships)
 * CLIENT_IN_GAME:           Game is active, taking turns
 * CLIENT_GAME_OVER:         Game has ended
 * CLIENT_ERROR:             An error occurred (connection lost, invalid state, etc.)
 */
typedef enum {
    CLIENT_INIT,
    CLIENT_CONNECTED,
    CLIENT_WAITING_FOR_OPPONENT,
    CLIENT_BOARD_SETUP,
    CLIENT_IN_GAME,
    CLIENT_GAME_OVER,
    CLIENT_ERROR
} ClientState;

/*
 * Client struct - Represents the client's state and connection.
 *
 * Stores the client's connection info, game state, and both boards:
 * - own_board: contains the client's ships and records of incoming shots
 * - enemy_view: tracks what we know about the opponent's board (hits/misses)
 *
 * Members:
 *   fd - socket file descriptor for communication with server
 *   room_id - server-assigned room ID (set after joining/creating room)
 *   player_id - server-assigned player ID (0 or 1)
 *   name - player's chosen name (max 31 chars + null terminator)
 *   state - current phase of the client (see ClientState enum)
 *   own_board - local Board with our ships and their hit status
 *   enemy_view - local Board representing what we know about opponent's board
 */
typedef struct {
    int fd;
    int room_id;
    int player_id;
    char name[MAX_NAME_LEN];
    ClientState state;
    Board own_board;
    Board enemy_view;
} Client;

/*
 * client_init - Initialize a Client struct with default values.
 *
 * Sets up an empty Client in CLIENT_INIT state with uninitialized boards
 * and no connection. Must be called before any other client function.
 *
 * Parameters:
 *   client - pointer to the Client struct to initialize
 *
 * Returns:
 *   void
 */
void client_init(Client *client);

/*
 * client_connect_to_server - Connect the client to the server.
 *
 * Calls socket.c's connect_to_server() to establish a TCP connection.
 * On success, updates client->fd and changes state to CLIENT_CONNECTED.
 * On failure, sets state to CLIENT_ERROR.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *   port - port number to connect to
 *   hostname - server hostname (e.g., "localhost" or "127.0.0.1")
 *
 * Returns:
 *   0 on success
 *   -1 on connection failure
 */
int client_connect_to_server(Client *client, int port, const char *hostname);

/*
 * client_create_room - Send a create room request to the server.
 *
 * Prompts for a player name, sends MSG_CREATE_ROOM to the server,
 * and waits for MSG_ROOM_CREATED response containing room_id and player_id.
 * On success, updates client state to CLIENT_WAITING_FOR_OPPONENT.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (room created)
 *   -1 on failure (message error, invalid response, etc.)
 */
int client_create_room(Client *client);

/*
 * client_join_room - Send a join room request to the server.
 *
 * Prompts for a player name and room ID, sends MSG_JOIN_ROOM to the server,
 * and waits for MSG_JOIN_OK response.
 * On success, updates client state to CLIENT_WAITING_FOR_OPPONENT.
 * Returns -1 if room is invalid or closed.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (joined room)
 *   -1 on failure (room not found, message error, etc.)
 */
int client_join_room(Client *client);

/*
 * client_place_ships - Interactively place ships on the client's board.
 *
 * Prompts the user to place each ship (from game.h) on their board.
 * For each ship, asks for starting x/y coordinates and orientation (H/V).
 * Validates placement using utils.c's validate_ship_placement().
 * Displays the board after each placement.
 * Changes client state to CLIENT_BOARD_SETUP.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (all ships placed)
 *   -1 on error (EOF or failed placement)
 */
int client_place_ships(Client *client);

/*
 * client_submit_board - Send the client's board to the server.
 *
 * Sends MSG_SUBMIT_BOARD containing all ship placements from own_board.
 * Waits for server response (MSG_BOARD_OK or MSG_BOARD_INVALID).
 * If board is rejected, prompts to place ships again.
 * On acceptance, waits for MSG_GAME_START to begin play.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (board accepted and game starting)
 *   -1 on failure (message error, repeated rejected boards, etc.)
 */
int client_submit_board(Client *client);

/*
 * client_wait_for_opponent - Wait for the opponent to join the room.
 *
 * After creating or joining a room, this blocking function waits for the server
 * to send MSG_OPPONENT_JOINED, indicating that both players are now in the room.
 * This signals that board setup (ship placement) can begin.
 *
 * The function may receive MSG_WAITING in the interim while still waiting for
 * the opponent to connect.
 *
 * Protocol:
 * - Expects to receive: MSG_WAITING (while waiting) or MSG_OPPONENT_JOINED (opponent joined)
 * - MSG_OPPONENT_JOINED means room is full, time for board setup
 * - Different from MSG_GAME_START, which comes after board submission
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (opponent joined, room ready)
 *   -1 on failure (connection lost, invalid message, etc.)
 */
int client_wait_for_opponent(Client *client);

/*
 * client_play_game - Main game loop for the client.
 *
 * Enters the game phase. Repeatedly:
 * - Receives messages from server (MSG_YOUR_TURN, MSG_INCOMING_SHOT, MSG_SHOT_RESULT, etc.)
 * - Takes a shot if it's the client's turn
 * - Processes incoming shots and shot results
 * - Continues until MSG_GAME_OVER is received
 * Updates client state to CLIENT_IN_GAME, then CLIENT_GAME_OVER.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   0 on success (game ended normally)
 *   -1 on failure (connection lost, protocol error, etc.)
 */
int client_play_game(Client *client);

/*
 * client_disconnect - Close the connection and clean up.
 *
 * Sends MSG_DISCONNECT to the server, closes the socket, and resets
 * the Client to CLIENT_INIT state.
 *
 * Parameters:
 *   client - pointer to the Client struct
 *
 * Returns:
 *   void
 */
void client_disconnect(Client *client);

#endif /* CLIENT_H */
