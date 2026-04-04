#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils.h"
#include "protocol.h"
#include "game.h"

/*
 * send_message - Send a message over socket with loop for partial sends.
 *
 * Sockets may not send all data in one call, so we loop until all bytes
 * are sent. This ensures the complete Message structure reaches the server.
 *
 * Returns:
 *   0 on success (all sizeof(Message) bytes sent)
 *   -1 on failure (send error or sent 0 bytes unexpectedly)
 */
int send_message(int fd, Message *msg) {
    size_t total = sizeof(Message);
    size_t sent = 0;
    char *data = (char *)msg;

    while (sent < total) {
        ssize_t n = send(fd, data + sent, total - sent, 0);
        if (n < 0) {
            // send() failed
            return -1;
        }
        if (n == 0) {
            // Socket closed unexpectedly
            return -1;
        }
        sent += n;
    }

    return 0;
}

/*
 * receive_message - Receive a message from socket with loop for partial receives.
 *
 * Sockets may not deliver all data in one call, especially for larger structures.
 * We loop until we get all sizeof(Message) bytes. This ensures we have the
 * complete Message before returning.
 *
 * Returns:
 *   0 on success (all sizeof(Message) bytes received)
 *   -1 on failure (recv error, peer closed, or received 0 bytes unexpectedly)
 */
int receive_message(int fd, Message *msg) {
    size_t total = sizeof(Message);
    size_t received = 0;
    char *data = (char *)msg;

    while (received < total) {
        ssize_t n = recv(fd, data + received, total - received, 0);
        if (n < 0) {
            // recv() failed
            return -1;
        }
        if (n == 0) {
            // Peer closed connection
            return -1;
        }
        received += n;
    }

    return 0;
}

/*
 * get_player_input - Read and validate shot coordinates from player.
 *
 * Repeatedly prompts for x (column 0-9) and y (row 0-9) until valid input
 * is received. Invalid input is re-prompted.
 *
 * Returns:
 *   0 on success (valid coordinates stored)
 *   -1 on EOF or read failure
 */
int get_player_input(int *x, int *y) {
    int result;

    while (1) {
        printf("Enter coordinates (x y, both 0-9): ");
        result = scanf("%d %d", x, y);

        // Check for read failure
        if (result != 2) {
            if (result == EOF) {
                return -1;
            }
            // Clear the input buffer of any garbage
            while (getchar() != '\n')
                ;
            printf("Invalid input. Please enter two integers.\n");
            continue;
        }

        // Validate coordinates are in bounds
        if (in_bounds(*x, *y)) {
            return 0;  // Valid input
        }

        printf("Coordinates out of bounds. Please use 0-9.\n");
    }
}

/*
 * validate_ship_placement - Check if a ship placement is valid on the board.
 *
 * Converts the ShipPlacement into a Ship struct (copying all relevant fields)
 * and then uses the existing can_place_ship() function to validate.
 * Does not modify the board.
 *
 * Returns:
 *   1 if placement is valid
 *   0 if placement is invalid (out of bounds or overlaps)
 */
int validate_ship_placement(Board *board, ShipPlacement placement) {
    // Convert ShipPlacement to Ship for validation
    Ship ship;
    ship.x = placement.x;
    ship.y = placement.y;
    ship.length = placement.length;
    ship.orientation = placement.orientation;
    /* hits and sunk don't matter for validation, ship hasn't been placed yet */
    ship.hits = 0;
    ship.sunk = 0;

    return can_place_ship(board, ship);
}

/*
 * print_board - Display the game board with current state of all cells.
 *
 * Prints a 10x10 grid with:
 *   '.' = empty cell
 *   'S' = ship (only if reveal_ships is 1)
 *   'X' = hit
 *   'O' = miss
 *
 * The opponent's board should be printed with reveal_ships=0 to hide ships.
 * Your own board should be printed with reveal_ships=1 to see your ship placements.
 *
 * Returns:
 *   void
 */
void print_board(const Board *board, int reveal_ships) {
    // Print column headers
    printf("  ");
    for (int x = 0; x < BOARD_SIZE; x++) {
        printf("%d ", x);
    }
    printf("\n");

    // Print each row
    for (int y = 0; y < BOARD_SIZE; y++) {
        printf("%d ", y);  // Row number
        for (int x = 0; x < BOARD_SIZE; x++) {
            CellState cell = board->cells[y][x];

            // Determine what to display
            char display = '.';  // Default: empty
            if (cell == CELL_HIT) {
                display = 'X';
            } else if (cell == CELL_MISS) {
                display = 'O';
            } else if (cell == CELL_SHIP && reveal_ships) {
                display = 'S';
            }

            printf("%c ", display);
        }
        printf("\n");
    }
    printf("\n");
}
