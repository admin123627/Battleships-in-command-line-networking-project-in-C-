# define BOARD_SIZE 10
# define MAX_SHIPS 5

/* Enum representing state of each cell on board.
*/
typedef enum {
    CELL_EMPTY,
    CELL_SHIP,
    CELL_HIT,
    CELL_MISS
} CellState;

/* This structure represents a ship in the game of Battleship. 
It contatins information about how many times its been hit, where it is,
if it has been sunk, its orientation. The position is the back of the ship,
so if it horizontal and length 3 and at (1, 1), this it spans (1, 1), 1, 2), and (1, 3). 
*/
typedef struct {
    int x;
    int y;
    int length;
    char orientation; // 'H' for horizontal, 'V' for vertical
    int hits; // Number of hits taken
    int sunk; // 0 for not sunk, 1 for sunk
} Ship;

/* This is a structure representing the game board for each player. It has information
about the ships on the board and the state of each cell.
*/
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    Ship ships[MAX_SHIPS];
    int ships_placed; // May take away but for now is used to track if player is ready to play
} Board;

/* Stucture representing the game state.
*/
typedef struct {
    Board boards[2]; // Two players, each with their own board
    int current_turn; // Whose turn, 0 for player 1, 1 for player 2
    int game_started; // 0 for not started, 1 for started
    int game_over; // 0 for not over, 1 for over
} Game;




