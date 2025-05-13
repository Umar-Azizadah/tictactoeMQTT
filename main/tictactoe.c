#include <stdio.h>
#include <stdlib.h>

char board[3][3];
char currentPlayer = 'X';

void initializeBoard();
void printBoard();
void getPlayerMove();
int checkWinner();

void app_main() {
    int winner = 0;
    initializeBoard();

    printf("ESP32 Tic-Tac-Toe (Text UI)\n");
    printf("Player 1 = Human (Serial input)\n");
    printf("Player 2 = Bash Script (MQTT input)\n\n");

    while (!winner) {
        printBoard();
        if (currentPlayer == 'X') {
            printf("Human Player's turn: %c, enter row and column (0-2)\n", currentPlayer);
            getPlayerMove();
        } else {
            printf("Waiting for Player O's move via MQTT..\n");
            //mqtt (to be added)
        }

        winner = checkWinner();

        if (!winner) {
            currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
        }
    }

    printBoard();

    if (winner == 3) {
        printf("It's a draw!\n");
    } else {
        printf("Player %c wins!\n", winner == 1 ? 'X' : 'O');
    }
}

void initializeBoard() {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            board[i][j] = ' ';
        }
    }
}

void printBoard() {
    printf("\nCurrent board:\n\n");
    for (int i = 0; i < 3; i++) {
        printf(" %c | %c | %c \n", board[i][0], board[i][1], board[i][2]);
        if (i < 2)
            printf("---+---+---\n");
    }
    printf("\n");
}

int checkWinner() {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] == board[i][1] && board[i][1] == board[i][2] && board[i][0] != ' ')
            return (board[i][0] == 'X') ? 1 : 2;
        if (board[0][i] == board[1][i] && board[1][i] == board[2][i] && board[0][i] != ' ')
            return (board[0][i] == 'X') ? 1 : 2;
    }

    if (board[0][0] == board[1][1] && board[1][1] == board[2][2] && board[0][0] != ' ')
        return (board[0][0] == 'X') ? 1 : 2;
    if (board[0][2] == board[1][1] && board[1][1] == board[2][0] && board[0][2] != ' ')
        return (board[0][2] == 'X') ? 1 : 2;

    int draw = 1;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            if (board[i][j] == ' ')
                draw = 0;

    return draw ? 3 : 0;
}

void getPlayerMove() {
    int row, col;
    while (1) {
        if (scanf("%d %d", &row, &col) != 2) {
            printf("Invalid input, enter two numbers.\n");
            while (getchar() != '\n');
            continue;
        }
        if (row >= 0 && row < 3 && col >= 0 && col < 3 && board[row][col] == ' ') {
            board[row][col] = currentPlayer;
            break;
        } else {
            printf("Invalid move, try again.\n");
        }
    }
}
