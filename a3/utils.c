#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "utils.h"
#include "protocol.h"
#include "game.h"

// Send a message to the server. Keeps looping until all bytes are sent
// Returns 0 if OK, -1 if something goes wrong
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

// Receive a message from the server. Loops until we get all the bytes
// Returns 0 if OK, -1 if the connection dies
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

// Asks player for coordinates. Keeps asking until they give valid numbers between 0-9
// Returns 0 if OK, -1 if EOF
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

// Check if a ship placement is OK. Converts ShipPlacement to Ship and validates it
// Returns 1 if good to place, 0 if not
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

// Print out the board with current state. X = hit, O = miss, S = ship (if revealed)
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

// Check if we already took a shot at this spot (hit or miss)
// Returns 1 if yes (can't shoot again), 0 if no (valid new shot)
int shot_already_taken(const Board *enemy_view, int x, int y) {
    if (!in_bounds(x, y)) {
        return 0;  /* Out of bounds is already handled by get_player_input */
    }
    CellState cell = enemy_view->cells[y][x];
    return (cell == CELL_HIT || cell == CELL_MISS);
}
