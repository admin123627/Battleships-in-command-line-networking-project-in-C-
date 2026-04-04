#ifndef UTILS_H
#define UTILS_H

#include "protocol.h"
#include "game.h"

/*
 * send_message - Send a message to the server over the socket.
 *
 * Sends a complete Message structure over the given socket file descriptor.
 * The message is transmitted with all its fields.
 *
 * Parameters:
 *   fd - socket file descriptor to send the message to
 *   msg - pointer to the Message to send
 *
 * Returns:
 *   On success: number of bytes sent (should equal sizeof(Message))
 *   On failure: -1 (with errno set by send())
 */
int send_message(int fd, Message *msg);

/*
 * receive_message - Receive a message from the server over the socket.
 *
 * Receives a complete Message structure from the given socket file descriptor.
 * Blocks until exactly sizeof(Message) bytes are received.
 *
 * Parameters:
 *   fd - socket file descriptor to receive the message from
 *   msg - pointer to write the received Message into
 *
 * Returns:
 *   On success: number of bytes received (should equal sizeof(Message))
 *   On failure: -1 (with errno set by recv())
 *   On disconnect: 0 (peer closed connection)
 */
int receive_message(int fd, Message *msg);

/*
 * get_player_input - Prompt the player to enter shot coordinates.
 *
 * Repeatedly prompts the player to enter two integers (x and y coordinates)
 * in the valid range [0, BOARD_SIZE-1]. Re-prompts on invalid input.
 *
 * Parameters:
 *   x - pointer to store the column coordinate (0-9)
 *   y - pointer to store the row coordinate (0-9)
 *
 * Returns:
 *   0 on success (valid coordinates stored in x and y)
 *   -1 on error (e.g., EOF or read failure)
 */
int get_player_input(int *x, int *y);

/*
 * validate_ship_placement - Check if a ship can be placed on the board.
 *
 * Verifies that the ship placement is valid by checking:
 * - The placement is within board bounds
 * - All cells occupied by the ship are empty (no overlaps)
 * Uses existing game logic functions (in_bounds, can_place_ship).
 *
 * Parameters:
 *   board - pointer to the Board to check placement on
 *   placement - the ShipPlacement to validate
 *
 * Returns:
 *   1 if placement is valid (can be placed)
 *   0 if placement is invalid (out of bounds or overlaps with existing ship)
 */
int validate_ship_placement(Board *board, ShipPlacement placement);

/*
 * print_board - Display the game board to the player.
 *
 * Prints a formatted 10x10 board showing current cell states.
 * If reveal_ships is 1, displays ship positions (use for your own board).
 * If reveal_ships is 0, hides ships (use for opponent's board).
 * Shows hits (X), misses (O), ships (S), and empty cells (.).
 *
 * Parameters:
 *   board - pointer to the Board to display
 *   reveal_ships - 1 to show ship locations, 0 to hide them
 *
 * Returns:
 *   void
 */
void print_board(const Board *board, int reveal_ships);

/*
 * shot_already_taken - Check if a shot has already been fired at a cell.
 *
 * Checks the enemy_view board to see if the given coordinates have already
 * been targeted (marked as CELL_HIT or CELL_MISS). Used for client-side
 * validation to prevent duplicate shots.
 *
 * Parameters:
 *   enemy_view - pointer to the Board representing the opponent's board view
 *   x - column coordinate (0-9)
 *   y - row coordinate (0-9)
 *
 * Returns:
 *   1 if the cell has already been shot at (CELL_HIT or CELL_MISS)
 *   0 if the cell is empty (CELL_EMPTY) or out of bounds
 */
int shot_already_taken(const Board *enemy_view, int x, int y);

#endif /* UTILS_H */
