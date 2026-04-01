#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
    run_server(listen_fd);

    /* Clean up */
    close(listen_fd);
    printf("Server shut down.\n");

    return 0;
}
