#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include "game.h"
#include "protocol.h"

#define PORT 9999
#define MAX_CLIENTS 20
#define MAX_ROOMS 10
#define ROOM_SIZE 2

/* Represents one connected client on the server. */
typedef struct {
    int socket_fd; // socket file descriptor for this client
    int is_connected; // 1 if this slot is active, 0 if empty

    int assigned_room_id; // room ID assigned by server (-1 if not in room)
    int assigned_player_id; // player ID within room (0 or 1, -1 if not assigned)

    char name[MAX_NAME_LEN]; // player's chosen name
} Client;

/* Represents one game room on the server. */
typedef struct {
    int room_id; // unique room number
    int is_active; // 1 if room exists, 0 if empty
    int current_player_count; // current number of players in room (0-2)
    int is_game_started; // 1 once gameplay begins

    int player_boards_submitted[ROOM_SIZE]; // whether each player submitted a valid board

    Client *connected_players[ROOM_SIZE]; // pointers to the two clients in this room
    Game game; // authoritative game state
} Room;

/* Core server loop */
void run_server(int listen_fd);

/* Connection handling */
void handle_new_connection(int listen_fd, Client clients[]);
void remove_client(Client clients[], Room rooms[], int client_index);

/* Messaging */
int send_message(int fd, Message *msg);
int receive_message(int fd, Message *msg);

/* Main dispatcher */
void handle_client_message(Client clients[], Room rooms[], int client_index);

/* Room management */
int create_room(Room rooms[]);
int join_room(Room rooms[], Client *client, int room_id);
Room *find_room(Room rooms[], int room_id);

/* Game setup */
void handle_submit_board(Room *room, Client *client, Message *msg);

/* Gameplay */
void handle_shot(Room *room, Client *client, Message *msg);
Client *get_opponent(Room *room, Client *client);

/* Utility */
void send_error(int fd, const char *msg);

#endif