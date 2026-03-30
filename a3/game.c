#include <stdio.h>
#include "game.h"

int in_bounds(int x, int y) {
    return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
}

void init_board(Board *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->cells[i][j] = CELL_EMPTY;
        }
    }
    board->ships_placed = 0;
}

void init_game(Game *game) {
    for (int i = 0; i < 2; i++) {
        init_board(&game->boards[i]);
    }
    game->current_turn = 0;
    game->game_started = 0;
    game->game_over = 0;
}

int can_place_ship(Board *board, Ship ship) {
	int dx = (ship.orientation == HORIZONTAL) ? 1 : 0;
	int dy = (ship.orientation == VERTICAL) ? 1 : 0;
	for (int i = 0; i < ship.length; i++) {
		int col = ship.x + i * dx;
        int row = ship.y + i * dy;
        // Check if the position is out of bounds or already occupied
        if (!in_bounds(col, row) || board->cells[row][col] != CELL_EMPTY) {
            return 0;
        }
	}
	return 1;
}	

int place_ship(Board *board, Ship ship) {
    if (!can_place_ship(board, ship)) {
        return 0; // Cannot place ship
    }
    int dx = (ship.orientation == HORIZONTAL) ? 1 : 0;
    int dy = (ship.orientation == VERTICAL) ? 1 : 0;
    for (int i = 0; i < ship.length; i++) {
        int col = ship.x + i * dx;
        int row = ship.y + i * dy;
        board->cells[row][col] = CELL_SHIP; // Changed state of cell to indicate a ship is there
    }
    ship.hits = 0; // Initialize hits to 0
    ship.sunk = 0; // Initialize sunk to 0
    board->ships[board->ships_placed] = ship; // Add ship to the board's list of ships
    board->ships_placed++; // Increment the count of ships placed
    return 1;
}

Ship *get_ship_at(Board *board, int x, int y) {
    for (int i = 0; i < board->ships_placed; i++) {
        Ship *ship = &board->ships[i];
        int dx = (ship->orientation == HORIZONTAL) ? 1 : 0;
        int dy = (ship->orientation == VERTICAL) ? 1 : 0;
        for (int j = 0; j < ship->length; j++) {
            int col = ship->x + j * dx;
            int row = ship->y + j * dy;
            if (col == x && row == y) {
                return ship; // Found the ship at the given coordinates
            }
        }
    }
    return NULL; // No ship found at the given coordinates
}

ShotResult take_shot(Board *board, int x, int y) {
    if (!in_bounds(x, y)) {
        return SHOT_INVALID; // Out of bounds
    }
    if (board->cells[y][x] == CELL_EMPTY) {
        board->cells[y][x] = CELL_MISS;
        return SHOT_MISS; // Miss
    } else if (board->cells[y][x] == CELL_SHIP) {
        board->cells[y][x] = CELL_HIT;
        Ship *ship = get_ship_at(board, x, y);
        if (ship) {
            ship->hits++;
            if (ship->hits == ship->length) {
                ship->sunk = 1; // Mark the ship as sunk
                return SHOT_SUNK; // Sunk
            }
        }
        return SHOT_HIT; // Hit
    } else {
        return SHOT_INVALID; // Already hit or missed
    }
}

int all_ships_sunk(Board *board) {
    for (int i = 0; i < board->ships_placed; i++) {
        if (!board->ships[i].sunk) {
            return 0; // At least one ship is not sunk
        }
    }
    return 1; // All ships are sunk
}
