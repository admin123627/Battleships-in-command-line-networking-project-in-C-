#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game.h"

#define MAX_NAME_LEN 32
#define MAX_ERROR_LEN 128

/* One ship placement used when a client submits a full board setup. */
typedef struct {
    int x;                      // starting column
    int y;                      // starting row
    int length;                 // ship length
    Orientation orientation;    // HORIZONTAL or VERTICAL
} ShipPlacement;

/* All message types exchanged between client and server. */
typedef enum {
    /* Room setup */
    MSG_CREATE_ROOM,     // Client -> Server
    MSG_ROOM_CREATED,    // Server -> Client
    MSG_JOIN_ROOM,       // Client -> Server
    MSG_JOIN_OK,         // Server -> Client
    MSG_WAITING,         // Server -> Client (waiting for opponent to join)
    MSG_OPPONENT_JOINED, // Server -> Client (both players in room, board setup can begin)
    MSG_GAME_START,      // Server -> Client (both boards submitted, gameplay begins)

    /* Board setup */
    MSG_SUBMIT_BOARD,    // Client -> Server
    MSG_BOARD_OK,        // Server -> Client
    MSG_BOARD_INVALID,   // Server -> Client

    /* Turn flow */
    MSG_YOUR_TURN,       // Server -> Client
    MSG_WAIT_TURN,       // Server -> Client
    MSG_SHOOT,           // Client -> Server
    MSG_SHOT_RESULT,     // Server -> Client
    MSG_INCOMING_SHOT,   // Server -> Client

    /* End / errors */
    MSG_GAME_OVER,       // Server -> Client
    MSG_OPPONENT_LEFT,   // Server -> Client (opponent disconnected, game ended immediately)
    MSG_ERROR,           // Server -> Client
    MSG_DISCONNECT       // Either direction
} MessageType;

/* Fixed-size message struct for all communication. */
typedef struct {
    MessageType type;

    int room_id;
    int player_id;

    /* Used for shooting */
    int x;                      // column
    int y;                      // row
    ShotResult shot_result;     // SHOT_HIT, SHOT_MISS, etc.

    /* Used for game over */
    int winner_id;

    /* Used for board submission */
    ShipPlacement ships[MAX_SHIPS];

    /* Optional text */
    char name[MAX_NAME_LEN];
    char error_msg[MAX_ERROR_LEN];
} Message;

#endif
