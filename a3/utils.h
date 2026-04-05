#ifndef UTILS_H
#define UTILS_H

#include "protocol.h"
#include "game.h"

// Send a message to the server. Returns 0 if OK, -1 if it fails
int send_message(int fd, Message *msg);

// Wait for a message from the server. Returns 0 if OK, -1 if connection dies
int receive_message(int fd, Message *msg);

// Ask the player where they want to shoot. Keeps asking until they give valid coords
int get_player_input(int *x, int *y);

// Check if a ship can go in that spot (not off board, not on another ship)
int validate_ship_placement(Board *board, ShipPlacement placement);

// Print the board. Set reveal_ships=1 to show our ships, 0 to hide enemy ships
void print_board(const Board *board, int reveal_ships);

// Check if we already shot at this spot. Returns 1 if yes, 0 if no
int shot_already_taken(const Board *enemy_view, int x, int y);

#endif /* UTILS_H */
