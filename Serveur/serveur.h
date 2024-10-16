#ifndef SERVEUR_H
#define SERVEUR_H

// Librairies
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// Constants
#define TIME_OUT_TIME 30
#define USERS_FILE "users.dat"
#define MAX_USERS 1000
#define MAX_GAMES_PER_PLAYER 5
#define BOARD_SIZE 12 // Nombre total de trou sur le plateau de jeu
#define PLAYER_PITS 6 // Nombre de trou par joueur
#define MAX_PLAYERS 100
#define MAX_GAMES 100
#define INITIAL_SEEDS 4 // Nombre de graine par trou au début du jeu
#define BUFFER_SIZE 1024

// Codes couleur
#define RESET "\x1b[0m"
#define RED "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define CYAN "\x1b[36m"
#define MAGENTA "\x1b[35m"
#define BLUE "\x1b[34m"

// Structures
typedef struct player_t player_t;
typedef struct game_t game_t;

struct player_t
{
    int sockfd;
    char pseudo[32];
    pthread_t thread;
    int connected;
    pthread_mutex_t player_mutex;
    game_t *games[MAX_GAMES_PER_PLAYER];
    int game_count;
    int challenge_sent;
    int challenge_received;
    player_t *challenger;
    player_t *challengee;
    // Statistiques du joueur
    int wins;
    int losses;
    int draws;
};

struct game_t
{
    int game_id;
    player_t *player1;
    player_t *player2;
    int board[BOARD_SIZE];
    int turn;
    int game_over;
    pthread_mutex_t game_mutex;
    // Scores par joueur dans la partie
    int player1_score;
    int player2_score;
    int waiting_reconnect;
};

typedef struct user_credentials_t
{
    char pseudo[32];
    char password[128];
} user_credentials_t;

// Prototypes
void *client_handler(void *arg);
void broadcast_to_all(char *message, player_t *sender);
void send_private_message(player_t *sender, const char *target_pseudo, const char *message);
void chat_in_game(player_t *player, int game_id, const char *message);
void handle_command(player_t *player, char *command);
void list_connected_players(player_t *player);
void show_help(player_t *player);
void handle_player_disconnect(player_t *player);
void *wait_for_reconnection(void *arg);
void challenge_player(player_t *player, const char *target_pseudo);
void accept_challenge(player_t *player);
void refuse_challenge(player_t *player);
void remove_challenge(player_t *player);
void init_board(int board[]);
void print_board(int sockfd, int player_id, player_t *current_player, player_t *other_player, int board[], int game_id, game_t *game);
void display_board(player_t *player, int game_id);
int make_move(int player_id, int pit, player_t *player, int board[], game_t *game);
void make_move_command(player_t *player, int game_id, int move);
void abandon_game(player_t *player, int game_id);
int check_game_end(int board[]);
void end_game(game_t *game);
void remove_game_from_player(player_t *player, game_t *game);


#endif