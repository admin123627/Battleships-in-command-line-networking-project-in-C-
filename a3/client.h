#ifndef CLIENT_H
#define CLIENT_H

#include "protocol.h"
#include "game.h"

/* Different states the client goes through
 * START -> CONNECTED -> WAITING -> BOARD_SETUP -> IN_GAME -> GAME_OVER
 * Also ERROR if something goes wrong at any point
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

/* Holds all the client's info: socket, room ID, boards, etc
 * own_board - our ships and where opponent has shot
 * enemy_view - what we know about opponent's board (hits and misses)
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

// Set up a fresh client struct before doing anything else
void client_init(Client *client);

// Connect to the server. Returns 0 if OK, -1 if it fails
int client_connect_to_server(Client *client, int port, const char *hostname);

// Ask player for their name and create a new room. Returns 0 if OK
int client_create_room(Client *client);

// Ask player for room ID and join an existing room. Returns 0 if OK
int client_join_room(Client *client);

// Let player place all their ships on the board. Returns 0 when done
int client_place_ships(Client *client);

// Send board to server and wait for it to be approved. Returns 0 if OK
int client_submit_board(Client *client);

// Sit tight and wait for the other player to join the room. Returns 0 when they do
int client_wait_for_opponent(Client *client);

// Main game loop - keep getting messages and taking turns until game ends
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
