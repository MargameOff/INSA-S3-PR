#include "serveur.h"

player_t *players[MAX_PLAYERS];
int player_count = 0;
pthread_mutex_t players_mutex = PTHREAD_MUTEX_INITIALIZER;

game_t *games[MAX_GAMES];
int game_count = 0;
int game_id_counter = 1;
pthread_mutex_t games_mutex = PTHREAD_MUTEX_INITIALIZER;

user_credentials_t users[MAX_USERS];
int user_count = 0;
pthread_mutex_t users_file_mutex = PTHREAD_MUTEX_INITIALIZER;

void load_users()
{
    pthread_mutex_lock(&users_file_mutex);
    FILE *file = fopen(USERS_FILE, "rb");
    if (file != NULL)
    {
        fread(&user_count, sizeof(int), 1, file);
        fread(users, sizeof(user_credentials_t), user_count, file);
        fclose(file);
    }
    pthread_mutex_unlock(&users_file_mutex);
}

void save_users()
{
    pthread_mutex_lock(&users_file_mutex);
    FILE *file = fopen(USERS_FILE, "wb");
    if (file != NULL)
    {
        fwrite(&user_count, sizeof(int), 1, file);
        fwrite(users, sizeof(user_credentials_t), user_count, file);
        fclose(file);
    }
    pthread_mutex_unlock(&users_file_mutex);
}

int find_user_index(const char *pseudo)
{
    for (int i = 0; i < user_count; ++i)
    {
        if (strcmp(users[i].pseudo, pseudo) == 0)
        {
            return i;
        }
    }
    return -1;
}

int register_user(const char *pseudo, const char *password)
{
    if (user_count >= MAX_USERS)
    {
        return -1; // Trop d'utilisateurs
    }

    // Ajouter l'utilisateur
    strcpy(users[user_count].pseudo, pseudo);
    strcpy(users[user_count].password, password);
    user_count++;

    save_users();
    return 0;
}

int verify_user_password(const char *pseudo, const char *password)
{
    int index = find_user_index(pseudo);
    if (index == -1)
    {
        return -1; // Utilisateur non trouvé
    }

    if (strcmp(users[index].password, password) == 0)
    {
        return 0; // Mot de passe correct
    }
    else
    {
        return -2; // Mot de passe incorrect
    }
}

void broadcast_to_all(char *message, player_t *sender)
{
    pthread_mutex_lock(&players_mutex);
    for (int i = 0; i < player_count; ++i)
    {
        player_t *p = players[i];
        if (p->connected && p != sender)
        {
            send(p->sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&players_mutex);
}

/*
    Envoyer un message en privé à un joueur
*/
void send_private_message(player_t *sender, const char *target_pseudo, const char *message)
{
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&players_mutex);
    player_t *target_player = NULL;
    for (int i = 0; i < player_count; ++i)
    {
        if (strcmp(players[i]->pseudo, target_pseudo) == 0 && players[i]->connected)
        {
            target_player = players[i];
            break;
        }
    }
    pthread_mutex_unlock(&players_mutex);

    if (target_player == NULL)
    {
        snprintf(buffer, sizeof(buffer), RED "Le joueur %s n'est pas connecté.\n" RESET, target_pseudo);
        send(sender->sockfd, buffer, strlen(buffer), 0);
        return;
    }

    snprintf(buffer, sizeof(buffer), MAGENTA "[MP de %s] %s\n" RESET, sender->pseudo, message);
    send(target_player->sockfd, buffer, strlen(buffer), 0);

    snprintf(buffer, sizeof(buffer), MAGENTA "[MP à %s] %s\n" RESET, target_pseudo, message);
    send(sender->sockfd, buffer, strlen(buffer), 0);
}

/*
    Envoyer un message dans le chat de la partie
*/
void chat_in_game(player_t *player, int game_id, const char *message)
{
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&player->player_mutex);
    game_t *game = NULL;
    for (int i = 0; i < player->game_count; ++i)
    {
        if (player->games[i]->game_id == game_id)
        {
            game = player->games[i];
            break;
        }
    }
    pthread_mutex_unlock(&player->player_mutex);

    if (game != NULL)
    {
        player_t *other_player = (game->player1 == player) ? game->player2 : game->player1;
        snprintf(buffer, sizeof(buffer), MAGENTA "[Partie %d] %s: %s\n" RESET, game_id, player->pseudo, message);
        send(other_player->sockfd, buffer, strlen(buffer), 0);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'êtes pas dans la partie %d.\n" RESET, game_id);
        send(player->sockfd, buffer, strlen(buffer), 0);
    }
}

// Gestion des défis

/*
    Envoyer un défi
*/
void challenge_player(player_t *player, const char *target_pseudo)
{
    char buffer[BUFFER_SIZE];

    // On récupère l'adversaire
    pthread_mutex_lock(&players_mutex);
    player_t *target_player = NULL;
    for (int i = 0; i < player_count; ++i)
    {
        if (strcmp(players[i]->pseudo, target_pseudo) == 0 && players[i]->connected)
        {
            target_player = players[i];
            break;
        }
    }
    pthread_mutex_unlock(&players_mutex);

    // Le joueur n'est pas connecté
    if (target_player == NULL)
    {
        snprintf(buffer, sizeof(buffer), RED "Le joueur %s n'est pas connecté.\n" RESET, target_pseudo);
        send(player->sockfd, buffer, strlen(buffer), 0);
        return;
    }

    // On se défie soi-même
    if (target_player == player)
    {
        snprintf(buffer, sizeof(buffer), RED "Vous ne pouvez pas vous défier vous-même.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);
        return;
    }

    pthread_mutex_lock(&player->player_mutex);
    pthread_mutex_lock(&target_player->player_mutex);

    // On ne peut pas avoir plusieurs défi à la fois.
    if (player->challenge_sent || player->challenge_received)
    {
        snprintf(buffer, sizeof(buffer), RED "Vous avez déjà un défi en cours.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);
        pthread_mutex_unlock(&target_player->player_mutex);
        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    if (target_player->challenge_received || target_player->challenge_sent)
    {
        snprintf(buffer, sizeof(buffer), RED "Le joueur %s est déjà en défi.\n" RESET, target_pseudo);
        send(player->sockfd, buffer, strlen(buffer), 0);
        pthread_mutex_unlock(&target_player->player_mutex);
        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    player->challenge_sent = 1;
    player->challengee = target_player;
    target_player->challenge_received = 1;
    target_player->challenger = player;

    snprintf(buffer, sizeof(buffer), YELLOW "%s vous a défié en duel ! Tapez /accepter pour accepter ou /refuser pour refuser.\n" RESET, player->pseudo);
    send(target_player->sockfd, buffer, strlen(buffer), 0);

    snprintf(buffer, sizeof(buffer), GREEN "Défi envoyé à %s.\n" RESET, target_pseudo);
    send(player->sockfd, buffer, strlen(buffer), 0);

    pthread_mutex_unlock(&target_player->player_mutex);
    pthread_mutex_unlock(&player->player_mutex);
}

/*
    Accepter un défi
*/
void accept_challenge(player_t *player)
{
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&player->player_mutex);

    // Aucun défi en attente
    if (!player->challenge_received || player->challenger == NULL)
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'avez aucun défi à accepter.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);
        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    player_t *challenger = player->challenger;

    pthread_mutex_lock(&challenger->player_mutex);

    // Créer une nouvelle partie
    game_t *new_game = (game_t *)malloc(sizeof(game_t));

    // On récupère l'id à partir du compteur global
    pthread_mutex_lock(&games_mutex);
    new_game->game_id = game_id_counter++;
    pthread_mutex_unlock(&games_mutex);

    new_game->player1 = challenger;
    new_game->player2 = player;
    init_board(new_game->board);
    new_game->turn = 0; // Le challenger commence
    new_game->game_over = 0;
    new_game->waiting_reconnect = 0;
    pthread_mutex_init(&new_game->game_mutex, NULL);
    new_game->player1_score = 0;
    new_game->player2_score = 0;

    // On ajoute la partie à la liste des parties
    pthread_mutex_lock(&games_mutex);
    games[game_count++] = new_game;
    pthread_mutex_unlock(&games_mutex);

    // Ajouter la partie aux joueurs
    challenger->games[challenger->game_count++] = new_game;
    player->games[player->game_count++] = new_game;

    // Réinitialiser les défis
    challenger->challenge_sent = 0;
    challenger->challengee = NULL;
    player->challenge_received = 0;
    player->challenger = NULL;

    // Informer les joueurs
    snprintf(buffer, sizeof(buffer), GREEN "Défi accepté. La partie %d commence !\n" RESET, new_game->game_id);
    send(challenger->sockfd, buffer, strlen(buffer), 0);
    send(player->sockfd, buffer, strlen(buffer), 0);

    // Envoyer le plateau initial aux joueurs
    print_board(challenger->sockfd, 0, challenger, player, new_game->board, new_game->game_id, new_game);
    print_board(player->sockfd, 1, player, challenger, new_game->board, new_game->game_id, new_game);

    // Informer le joueur qui commence
    snprintf(buffer, sizeof(buffer), GREEN "C'est à vous de jouer.\n" RESET);
    send(challenger->sockfd, buffer, strlen(buffer), 0);

    pthread_mutex_unlock(&challenger->player_mutex);
    pthread_mutex_unlock(&player->player_mutex);
}

/*
    Refuser un défi
*/
void refuse_challenge(player_t *player)
{
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&player->player_mutex);

    if (!player->challenge_received || player->challenger == NULL)
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'avez aucun défi à refuser.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);
        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    player_t *challenger = player->challenger;

    pthread_mutex_lock(&challenger->player_mutex);

    // Informer le challenger
    snprintf(buffer, sizeof(buffer), RED "%s a refusé votre défi.\n" RESET, player->pseudo);
    send(challenger->sockfd, buffer, strlen(buffer), 0);

    // Réinitialiser les défis
    challenger->challenge_sent = 0;
    challenger->challengee = NULL;
    player->challenge_received = 0;
    player->challenger = NULL;

    // Informer le joueur
    snprintf(buffer, sizeof(buffer), GREEN "Vous avez refusé le défi de %s.\n" RESET, challenger->pseudo);
    send(player->sockfd, buffer, strlen(buffer), 0);

    pthread_mutex_unlock(&challenger->player_mutex);
    pthread_mutex_unlock(&player->player_mutex);
}

/*
    Retirer les challenges
*/
void remove_challenge(player_t *player)
{
    // Le mutex du joueur est déjà vérouillé
    player_t *other_player = NULL;
    int has_challenge = 0;

    if (player->challenge_sent && player->challengee != NULL)
    {
        other_player = player->challengee;
        has_challenge = 1;
    }
    else if (player->challenge_received && player->challenger != NULL)
    {
        other_player = player->challenger;
        has_challenge = 1;
    }

    if (has_challenge)
    {
        pthread_mutex_lock(&other_player->player_mutex);

        // Informer l'autre joueur
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), RED "%s s'est déconnecté. Le défi est annulé.\n" RESET, player->pseudo);
        send(other_player->sockfd, buffer, strlen(buffer), 0);

        // Réinitialiser les défis pour les deux joueurs
        player->challenge_sent = 0;
        player->challenge_received = 0;
        player->challenger = NULL;
        player->challengee = NULL;

        other_player->challenge_sent = 0;
        other_player->challenge_received = 0;
        other_player->challenger = NULL;
        other_player->challengee = NULL;

        pthread_mutex_unlock(&other_player->player_mutex);
    }
    else
    {
        player->challenge_sent = 0;
        player->challenge_received = 0;
        player->challenger = NULL;
        player->challengee = NULL;
    }
}

// ********************************************************************************* //

// Gestion du jeu Awale

/*
    Initialisation du plateau de jeu
*/
void init_board(int board[])
{
    for (int i = 0; i < BOARD_SIZE; ++i)
    {
        board[i] = INITIAL_SEEDS;
    }
}

/*
    Affichage du plateau de jeu
*/
void print_board(int sockfd, int player_id, player_t *current_player, player_t *other_player, int board[], int game_id, game_t *game)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    // Afficher le plateau de façon propre et ajouter les scores
    snprintf(buffer, sizeof(buffer), "\n");
    send(sockfd, buffer, strlen(buffer), 0);

    int current_player_score = (player_id == 0) ? game->player1_score : game->player2_score;
    int other_player_score = (player_id == 0) ? game->player2_score : game->player1_score;

    snprintf(buffer, sizeof(buffer), YELLOW "[Partie %d] Adversaire (%s) : %d points\n\n" RESET, game_id, other_player->pseudo, other_player_score);
    send(sockfd, buffer, strlen(buffer), 0);

    if (player_id == 0)
    {
        // Affichage pour le joueur 1

        // Partie supérieure du plateau (joueur 2)
        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   |");
        for (int i = BOARD_SIZE - 1; i >= PLAYER_PITS; --i)
        {
            char pit[16];
            snprintf(pit, sizeof(pit), " %3d |", board[i]);
            strcat(buffer, pit);
        }
        strcat(buffer, "\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        // Partie inférieure du plateau (joueur 1)
        snprintf(buffer, sizeof(buffer), "   |");
        for (int i = 0; i < PLAYER_PITS; ++i)
        {
            char pit[16];
            snprintf(pit, sizeof(pit), " %3d |", board[i]);
            strcat(buffer, pit);
        }
        strcat(buffer, "\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "    [0]   [1]   [2]   [3]   [4]   [5]\n\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), CYAN "      Toi (%s) : %d points\n" RESET, current_player->pseudo, current_player_score);
        send(sockfd, buffer, strlen(buffer), 0);
    }
    else
    {
        // Affichage pour le joueur 2

        // Partie supérieure du plateau (joueur 1)
        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   |");
        for (int i = PLAYER_PITS - 1; i >= 0; --i)
        {
            char pit[16];
            snprintf(pit, sizeof(pit), " %3d |", board[i]);
            strcat(buffer, pit);
        }
        strcat(buffer, "\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        // Partie inférieure du plateau (joueur 2)
        snprintf(buffer, sizeof(buffer), "   |");
        for (int i = PLAYER_PITS; i < BOARD_SIZE; ++i)
        {
            char pit[16];
            snprintf(pit, sizeof(pit), " %3d |", board[i]);
            strcat(buffer, pit);
        }
        strcat(buffer, "\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "   +-----+-----+-----+-----+-----+-----+\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), "    [0]   [1]   [2]   [3]   [4]   [5]\n\n");
        send(sockfd, buffer, strlen(buffer), 0);

        snprintf(buffer, sizeof(buffer), CYAN "Toi (%s) : %d points\n" RESET, current_player->pseudo, current_player_score);
        send(sockfd, buffer, strlen(buffer), 0);
    }
}

void display_board(player_t *player, int game_id)
{
    char buffer[BUFFER_SIZE];
    pthread_mutex_lock(&player->player_mutex);
    game_t *game = NULL;
    for (int i = 0; i < player->game_count; ++i)
    {
        if (player->games[i]->game_id == game_id)
        {
            game = player->games[i];
            break;
        }
    }
    pthread_mutex_unlock(&player->player_mutex);

    if (game != NULL)
    {
        pthread_mutex_lock(&game->game_mutex);
        int player_id = (game->player1 == player) ? 0 : 1;
        player_t *other_player = (game->player1 == player) ? game->player2 : game->player1;
        print_board(player->sockfd, player_id, player, other_player, game->board, game_id, game);
        pthread_mutex_unlock(&game->game_mutex);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'êtes pas dans la partie %d.\n" RESET, game_id);
        send(player->sockfd, buffer, strlen(buffer), 0);
    }
}

/*
    Jouer un coup
*/
int make_move(int player_id, int pit, player_t *player, int board[], game_t *game)
{
    int start = player_id == 0 ? 0 : PLAYER_PITS;
    int end = player_id == 0 ? PLAYER_PITS - 1 : BOARD_SIZE - 1;

    // Vérification que le joueur joue dans sa propre rangée
    if (pit < start || pit > end)
    {
        return 0; // Mouvement invalide : hors de la rangée du joueur
    }

    int seeds = board[pit];
    if (seeds == 0)
    {
        return 0; // Mouvement invalide : trou vide
    }

    board[pit] = 0; // On vide le trou choisi
    int current_pit = pit;

    // Semer les graines dans les trous suivants
    while (seeds > 0)
    {
        current_pit = (current_pit + 1) % BOARD_SIZE;
        board[current_pit]++;
        seeds--;
    }

    int captured_seeds = 0; // Initialiser le nombre de graines capturées

    // Vérifier si la capture est possible
    if ((player_id == 0 && current_pit >= PLAYER_PITS) || (player_id == 1 && current_pit < PLAYER_PITS))
    {
        // Remonter dans les trous adverses et capturer si les conditions sont remplies
        while ((player_id == 0 && current_pit >= PLAYER_PITS) || (player_id == 1 && current_pit < PLAYER_PITS))
        {
            if (board[current_pit] == 2 || board[current_pit] == 3)
            {
                captured_seeds += board[current_pit];
                board[current_pit] = 0;
                current_pit--;
                if (current_pit < 0)
                    current_pit = BOARD_SIZE - 1;
            }
            else
            {
                break;
            }
        }
    }

    // Mettre à jour le score du joueur dans la partie
    if (player_id == 0)
    {
        game->player1_score += captured_seeds;
    }
    else
    {
        game->player2_score += captured_seeds;
    }

    return 1; // Mouvement valide
}

/*
    Décider du joueur qui doit jouer et récupérer le coup joué
*/
void make_move_command(player_t *player, int game_id, int move)
{
    char buffer[BUFFER_SIZE];

    // On récupère la partie à partir du numéro de partie donné
    pthread_mutex_lock(&player->player_mutex);
    game_t *game = NULL;
    for (int i = 0; i < player->game_count; ++i)
    {
        if (player->games[i]->game_id == game_id)
        {
            game = player->games[i];
            break;
        }
    }
    pthread_mutex_unlock(&player->player_mutex);

    if (game != NULL)
    {
        pthread_mutex_lock(&game->game_mutex);

        if (game->game_over)
        {
            snprintf(buffer, sizeof(buffer), RED "La partie %d est terminée.\n" RESET, game_id);
            send(player->sockfd, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&game->game_mutex);
            return;
        }

        int player_id = (game->player1 == player) ? 0 : 1;
        player_t *other_player = (game->player1 == player) ? game->player2 : game->player1;

        if (player_id == game->turn)
        {
            if (move >= 0 && move <= 5)
            {
                int pit = move;
                // Conversion pour le joueur 2 (trous 6 à 11)
                if (player_id == 1)
                    pit += PLAYER_PITS;

                if (make_move(player_id, pit, player, game->board, game))
                {
                    char move_msg[BUFFER_SIZE];
                    snprintf(move_msg, sizeof(move_msg), BLUE "[Partie %d] %s a joué le trou %d.\n" RESET, game->game_id, player->pseudo, pit % PLAYER_PITS);
                    send(other_player->sockfd, move_msg, strlen(move_msg), 0);

                    // Envoyer le nouveau plateau aux deux joueurs
                    print_board(player->sockfd, player_id, player, other_player, game->board, game->game_id, game);
                    print_board(other_player->sockfd, 1 - player_id, other_player, player, game->board, game->game_id, game);

                    // Vérifier si la partie est terminée
                    if (check_game_end(game->board))
                    {
                        end_game(game);
                        pthread_mutex_unlock(&game->game_mutex);
                        return;
                    }

                    // Mise à jour du tour
                    game->turn = 1 - game->turn;

                    // Informer le prochain joueur que c'est son tour
                    snprintf(buffer, sizeof(buffer), GREEN "[Partie %d] C'est à vous de jouer.\n" RESET, game->game_id);
                    if (game->turn == 0)
                        send(game->player1->sockfd, buffer, strlen(buffer), 0);
                    else
                        send(game->player2->sockfd, buffer, strlen(buffer), 0);
                }
                else
                {
                    snprintf(buffer, sizeof(buffer), RED "Mouvement invalide. Essayez à nouveau.\n" RESET);
                    send(player->sockfd, buffer, strlen(buffer), 0);
                }
            }
            else
            {
                snprintf(buffer, sizeof(buffer), RED "Entrée invalide. Veuillez entrer un nombre entre 0 et 5.\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
            }
        }
        else
        {
            snprintf(buffer, sizeof(buffer), RED "Ce n'est pas votre tour de jouer dans la partie %d.\n" RESET, game_id);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }

        pthread_mutex_unlock(&game->game_mutex);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'êtes pas dans la partie %d.\n" RESET, game_id);
        send(player->sockfd, buffer, strlen(buffer), 0);
    }
}

/*
    Abandonner une partie
*/
void abandon_game(player_t *player, int game_id)
{
    char buffer[BUFFER_SIZE];

    pthread_mutex_lock(&player->player_mutex);
    if (player->game_count == 0)
    {
        snprintf(buffer, sizeof(buffer), RED "Vous n'avez aucune partie en cours pour abandonner.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);

        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    // On récupère la partie à abandonner
    game_t *game = NULL;
    for (int i = 0; i < player->game_count; ++i)
    {
        if (player->games[i]->game_id == game_id)
        {
            game = player->games[i];
            break;
        }
    }

    if (game == NULL)
    {
        snprintf(buffer, sizeof(buffer), RED "Cette partie n'existe pas.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);

        pthread_mutex_unlock(&player->player_mutex);
        return;
    }

    pthread_mutex_unlock(&player->player_mutex);

    pthread_mutex_lock(&game->game_mutex);

    if (!game->game_over)
    {
        player_t *other_player = (game->player1 == player) ? game->player2 : game->player1;

        // Verrouiller les mutex dans l'ordre déterminé
        pthread_mutex_lock(&player->player_mutex);
        pthread_mutex_lock(&other_player->player_mutex);

        // Mettre à jour les statistiques
        player->losses++;
        other_player->wins++;

        // Informer l'autre joueur
        snprintf(buffer, sizeof(buffer), RED "%s a abandonné la partie %d. Vous remportez la partie !\n" RESET, player->pseudo, game->game_id);
        send(other_player->sockfd, buffer, strlen(buffer), 0);

        // Envoyer une confirmation au joueur
        snprintf(buffer, sizeof(buffer), GREEN "Vous avez abandonné la partie %d.\n" RESET, game->game_id);
        send(player->sockfd, buffer, strlen(buffer), 0);

        // Déverrouiller les mutex des joueurs avant de retirer les parties ! (on les rebloque dedans)
        pthread_mutex_unlock(&player->player_mutex);
        pthread_mutex_unlock(&other_player->player_mutex);

        // Retirer la partie des deux joueurs
        remove_game_from_player(player, game);
        remove_game_from_player(other_player, game);

        // Marquer la partie comme terminée
        game->game_over = 1;

        // Nettoyer la partie
        pthread_mutex_unlock(&game->game_mutex);
        pthread_mutex_destroy(&game->game_mutex);
        free(game);
    }
    else
    {
        pthread_mutex_unlock(&game->game_mutex);
    }
}

/*
    Vérifie si la partie est terminée
*/
int check_game_end(int board[])
{
    // Vérifie si un des joueurs n'a plus de graines dans son camp
    int player1_seeds = 0, player2_seeds = 0;

    for (int i = 0; i < PLAYER_PITS; ++i)
    {
        player1_seeds += board[i];
    }

    for (int i = PLAYER_PITS; i < BOARD_SIZE; ++i)
    {
        player2_seeds += board[i];
    }

    if (player1_seeds == 0 || player2_seeds == 0)
    {
        return 1;
    }

    return 0;
}

void end_game(game_t *game)
{
    char buffer[BUFFER_SIZE];

    // Ajouter les graines restantes aux scores des joueurs
    int player1_remaining_seeds = 0;
    int player2_remaining_seeds = 0;

    // Somme des graines dans le camp de chaque joueur
    for (int i = 0; i < PLAYER_PITS; ++i)
    {
        player1_remaining_seeds += game->board[i];
    }

    for (int i = PLAYER_PITS; i < BOARD_SIZE; ++i)
    {
        player2_remaining_seeds += game->board[i];
    }

    // Mise à jour des scores
    game->player1_score += player1_remaining_seeds;
    game->player2_score += player2_remaining_seeds;

    // Envoyer le plateau final aux deux joueurs
    print_board(game->player1->sockfd, 0, game->player1, game->player2, game->board, game->game_id, game);
    print_board(game->player2->sockfd, 1, game->player2, game->player1, game->board, game->game_id, game);

    // Nettoyage du plateau
    memset(game->board, 0, sizeof(game->board));

    // Envoyer les résultats finaux
    snprintf(buffer, sizeof(buffer), GREEN "Fin de la partie %d !\n" RESET, game->game_id);
    send(game->player1->sockfd, buffer, strlen(buffer), 0);
    send(game->player2->sockfd, buffer, strlen(buffer), 0);

    // Déterminer et annoncer le gagnant
    int player1_total_score = game->player1_score;
    int player2_total_score = game->player2_score;

    if (player1_total_score > player2_total_score)
    {
        snprintf(buffer, sizeof(buffer), YELLOW "[Partie %d] %s a gagné la partie avec %d points !\n" RESET, game->game_id, game->player1->pseudo, player1_total_score);
        send(game->player1->sockfd, buffer, strlen(buffer), 0);
        send(game->player2->sockfd, buffer, strlen(buffer), 0);

        // Mettre à jour les statistiques
        pthread_mutex_lock(&game->player1->player_mutex);
        game->player1->wins++;
        pthread_mutex_unlock(&game->player1->player_mutex);

        pthread_mutex_lock(&game->player2->player_mutex);
        game->player2->losses++;
        pthread_mutex_unlock(&game->player2->player_mutex);
    }
    else if (player2_total_score > player1_total_score)
    {
        snprintf(buffer, sizeof(buffer), YELLOW "[Partie %d] %s a gagné la partie avec %d points !\n" RESET, game->game_id, game->player2->pseudo, player2_total_score);
        send(game->player1->sockfd, buffer, strlen(buffer), 0);
        send(game->player2->sockfd, buffer, strlen(buffer), 0);

        // Mettre à jour les statistiques
        pthread_mutex_lock(&game->player1->player_mutex);
        game->player1->losses++;
        pthread_mutex_unlock(&game->player1->player_mutex);

        pthread_mutex_lock(&game->player2->player_mutex);
        game->player2->wins++;
        pthread_mutex_unlock(&game->player2->player_mutex);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), YELLOW "[Partie %d] Match nul ! Les deux joueurs ont %d points.\n" RESET, game->game_id, player1_total_score);
        send(game->player1->sockfd, buffer, strlen(buffer), 0);
        send(game->player2->sockfd, buffer, strlen(buffer), 0);

        // Mettre à jour les statistiques
        pthread_mutex_lock(&game->player1->player_mutex);
        game->player1->draws++;
        pthread_mutex_unlock(&game->player1->player_mutex);

        pthread_mutex_lock(&game->player2->player_mutex);
        game->player2->draws++;
        pthread_mutex_unlock(&game->player2->player_mutex);
    }

    // Marquer la partie comme terminée
    game->game_over = 1;

    // Retirer la partie des joueurs
    remove_game_from_player(game->player1, game);
    remove_game_from_player(game->player2, game);

    // Nettoyer la partie
    pthread_mutex_destroy(&game->game_mutex);
    free(game);
}

/*
    On retire la partie de la lsite des parties du joueur
*/
void remove_game_from_player(player_t *player, game_t *game)
{
    pthread_mutex_lock(&player->player_mutex);
    int index = -1;
    for (int i = 0; i < player->game_count; ++i)
    {
        if (player->games[i]->game_id == game->game_id)
        {
            index = i;
            break;
        }
    }

    if (index != -1)
    {
        for (int i = index; i < player->game_count - 1; ++i)
        {
            player->games[i] = player->games[i + 1];
        }
        player->game_count--;
    }
    pthread_mutex_unlock(&player->player_mutex);
}

void handle_command(player_t *player, char *command)
{
    char buffer[BUFFER_SIZE];

    if (strncmp(command, "/defier ", 8) == 0)
    {
        challenge_player(player, command + 8);
    }
    else if (strcmp(command, "/accepter") == 0)
    {
        accept_challenge(player);
    }
    else if (strcmp(command, "/refuser") == 0)
    {
        refuse_challenge(player);
    }
    else if (strcmp(command, "/joueurs") == 0)
    {
        list_connected_players(player);
    }
    else if (strcmp(command, "/help") == 0)
    {
        show_help(player);
    }
    else if (strncmp(command, "/global ", 8) == 0)
    {
        snprintf(buffer, sizeof(buffer), CYAN "[Global] %s: %s\n" RESET, player->pseudo, command + 8);
        broadcast_to_all(buffer, player);
    }
    else if (strncmp(command, "/mp ", 4) == 0)
    {
        char *target_pseudo = strtok(command + 4, " ");
        char *msg = strtok(NULL, "");
        if (target_pseudo && msg)
        {
            send_private_message(player, target_pseudo, msg);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), RED "Format incorrect. Utilisez /mp <pseudo> <message>\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }
    }
    else if (strncmp(command, "/chat ", 6) == 0)
    {
        char *rest = command + 6;
        int game_id;
        char *message;
        if (sscanf(rest, "%d", &game_id) == 1)
        {
            message = strchr(rest, ' ');
            if (message)
            {
                message++; // On saute l'espace
                chat_in_game(player, game_id, message);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), RED "Format incorrect. Utilisez /chat <numéro de partie> <message>\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
            }
        }
        else
        {
            snprintf(buffer, sizeof(buffer), RED "Format incorrect. Utilisez /chat <numéro de partie> <message>\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }
    }
    else if (strncmp(command, "/play", 5) == 0)
    {
        int game_id, move;
        char *cmd_args = command + 5;
        while (*cmd_args == ' ')
            cmd_args++; // Ignorer les espaces

        if (sscanf(cmd_args, "%d %d", &game_id, &move) == 2)
        {
            // Le joueur souhaite jouer un coup
            make_move_command(player, game_id, move);
        }
        else if (sscanf(cmd_args, "%d", &game_id) == 1)
        {
            // Le joueur souhaite afficher le plateau
            display_board(player, game_id);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), RED "Format incorrect. Utilisez /play <numéro de partie> [<nombre de 0 à 5>]\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }
    }
    else if (strncmp(command, "/abandon ", 9) == 0)
    {
        char *rest = command + 9;
        int game_id;
        if (sscanf(rest, "%d", &game_id) == 1)
        {
            abandon_game(player, game_id);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), RED "Format incorrect. Utilisez /abandon <numéro de partie>\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }
    }
    else if (strcmp(command, "/quit") == 0)
    {
        handle_player_disconnect(player);
        pthread_exit(NULL);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), RED "Commande non reconnue. Tapez /help pour voir la liste des commandes.\n" RESET);
        send(player->sockfd, buffer, strlen(buffer), 0);
    }
}

void list_connected_players(player_t *player)
{
    char buffer[BUFFER_SIZE];
    char line[BUFFER_SIZE];
    pthread_mutex_lock(&players_mutex);
    snprintf(buffer, sizeof(buffer), CYAN "Joueurs connectés :\n" RESET);
    for (int i = 0; i < player_count; ++i)
    {
        player_t *p = players[i];
        if (p->connected)
        {
            pthread_mutex_lock(&p->player_mutex);
            snprintf(line, sizeof(line), "%s - V: %d | D: %d | N: %d\n", p->pseudo, p->wins, p->losses, p->draws);
            pthread_mutex_unlock(&p->player_mutex);
            strcat(buffer, line);
        }
    }
    pthread_mutex_unlock(&players_mutex);
    send(player->sockfd, buffer, strlen(buffer), 0);
}

void show_help(player_t *player)
{
    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer),
             CYAN "Commandes disponibles :\n"
                  "/defier <pseudo> - Défier un joueur\n"
                  "/accepter - Accepter un défi\n"
                  "/refuser - Refuser un défi\n"
                  "/joueurs - Lister les joueurs connectés\n"
                  "/global <message> - Envoyer un message au chat global\n"
                  "/mp <pseudo> <message> - Envoyer un message privé\n"
                  "/chat <numéro de partie> <message> - Envoyer un message dans une partie\n"
                  "/play <numéro de partie> [<nombre de 0 à 5>] - Afficher le plateau ou jouer dans une partie\n"
                  "/abandon <numéro de partie> - Abandonner la partie\n"
                  "/quit - Quitter le jeu\n"
                  "/help - Afficher cette aide\n" RESET);
    send(player->sockfd, buffer, strlen(buffer), 0);
}

void handle_player_disconnect(player_t *player)
{
    char buffer[BUFFER_SIZE];

    printf("handle_player_disconnect: Joueur %s se déconnecte.\n", player->pseudo);

    // Verrouiller le mutex du joueur
    pthread_mutex_lock(&player->player_mutex);

    if (!player->connected)
    {
        printf("handle_player_disconnect: Joueur %s déjà marqué comme déconnecté.\n", player->pseudo);
        pthread_mutex_unlock(&player->player_mutex);
        return; // Le joueur est déjà marqué comme déconnecté
    }

    player->connected = 0;

    // Informer les autres joueurs globalement
    snprintf(buffer, sizeof(buffer), RED "%s s'est déconnecté.\n" RESET, player->pseudo);
    broadcast_to_all(buffer, player);

    // Gérer les défis en attente
    remove_challenge(player);

    // Gérer les parties en cours
    for (int i = 0; i < player->game_count; ++i)
    {
        game_t *game = player->games[i];
        pthread_mutex_lock(&game->game_mutex);

        printf("handle_player_disconnect: Vérification de la partie %d pour le joueur %s.\n", game->game_id, player->pseudo);
        printf("handle_player_disconnect: game_over=%d, waiting_reconnect=%d\n", game->game_over, game->waiting_reconnect);

        if (!game->game_over && !game->waiting_reconnect)
        {
            printf("handle_player_disconnect: Entrée dans la condition pour la partie %d.\n", game->game_id);

            game->waiting_reconnect = 1;

            // Informer l'autre joueur
            player_t *other_player = (game->player1 == player) ? game->player2 : game->player1;

            pthread_mutex_lock(&other_player->player_mutex);
            snprintf(buffer, sizeof(buffer), RED "Votre adversaire %s s'est déconnecté. En attente de reconnexion pendant %d secondes...\n" RESET, player->pseudo, TIME_OUT_TIME);
            int bytes_sent = send(other_player->sockfd, buffer, strlen(buffer), 0);
            if (bytes_sent < 0)
            {
                perror("Erreur lors de l'envoi du message à l'autre joueur");
            }
            pthread_mutex_unlock(&other_player->player_mutex);

            // Lancer un thread pour gérer la reconnexion
            pthread_t reconnect_thread;
            pthread_create(&reconnect_thread, NULL, wait_for_reconnection, (void *)game);
            pthread_detach(reconnect_thread);
        }
        else
        {
            printf("handle_player_disconnect: Condition non satisfaite pour la partie %d.\n", game->game_id);
        }

        pthread_mutex_unlock(&game->game_mutex);
    }

    pthread_mutex_unlock(&player->player_mutex);

    close(player->sockfd);
}

void *wait_for_reconnection(void *arg)
{
    game_t *game = (game_t *)arg;
    player_t *player1 = game->player1;
    player_t *player2 = game->player2;
    player_t *disconnected_player;
    player_t *other_player;

    // Déterminer le joueur déconnecté
    pthread_mutex_lock(&player1->player_mutex);
    if (!player1->connected)
    {
        disconnected_player = player1;
        other_player = player2;
    }
    else
    {
        disconnected_player = player2;
        other_player = player1;
    }
    pthread_mutex_unlock(&player1->player_mutex);

    int wait_time = TIME_OUT_TIME; // Temps d'attente en secondes
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < wait_time; ++i)
    {
        sleep(1);
        pthread_mutex_lock(&disconnected_player->player_mutex);
        if (disconnected_player->connected)
        {
            // Le joueur s'est reconnecté
            pthread_mutex_unlock(&disconnected_player->player_mutex);

            pthread_mutex_lock(&game->game_mutex);
            game->waiting_reconnect = 0;
            pthread_mutex_unlock(&game->game_mutex);

            // Informer l'autre joueur que la partie reprend
            snprintf(buffer, sizeof(buffer), GREEN "%s s'est reconnecté. La partie %d reprend.\n" RESET, disconnected_player->pseudo, game->game_id);
            pthread_mutex_lock(&other_player->player_mutex);
            send(other_player->sockfd, buffer, strlen(buffer), 0);
            pthread_mutex_unlock(&other_player->player_mutex);

            // Réafficher le plateau pour les deux joueurs
            int player_id = (game->player1 == disconnected_player) ? 0 : 1;
            print_board(disconnected_player->sockfd, player_id, disconnected_player, other_player, game->board, game->game_id, game);
            print_board(other_player->sockfd, 1 - player_id, other_player, disconnected_player, game->board, game->game_id, game);

            // Informer le joueur que c'est son tour
            snprintf(buffer, sizeof(buffer), GREEN "C'est à vous de jouer.\n" RESET);
            if (game->turn == 0)
                send(game->player1->sockfd, buffer, strlen(buffer), 0);
            else
                send(game->player2->sockfd, buffer, strlen(buffer), 0);

            return NULL; // Fin du thread
        }
        pthread_mutex_unlock(&disconnected_player->player_mutex);
    }

    // Le joueur ne s'est pas reconnecté après le délai
    pthread_mutex_lock(&game->game_mutex);
    game->game_over = 1;
    game->waiting_reconnect = 0;
    pthread_mutex_unlock(&game->game_mutex);

    // Informer l'autre joueur que la partie est terminée
    snprintf(buffer, sizeof(buffer), RED "%s ne s'est pas reconnecté. Vous remportez la partie %d !\n" RESET, disconnected_player->pseudo, game->game_id);
    pthread_mutex_lock(&other_player->player_mutex);
    send(other_player->sockfd, buffer, strlen(buffer), 0);

    // Mettre à jour les statistiques
    other_player->wins++;
    pthread_mutex_unlock(&other_player->player_mutex);

    // Retirer la partie des deux joueurs
    remove_game_from_player(disconnected_player, game);
    remove_game_from_player(other_player, game);

    // Nettoyer la partie
    pthread_mutex_destroy(&game->game_mutex);
    free(game);

    return NULL;
}

// Thread traitant toutes les commandes d'un client
void *client_handler(void *arg)
{
    player_t *player = (player_t *)arg;
    char buffer[BUFFER_SIZE];
    int receive;

    // Envoyer un message de bienvenue
    snprintf(buffer, sizeof(buffer), GREEN "Bienvenue %s ! Tapez /help pour les commandes disponibles.\n" RESET, player->pseudo);
    send(player->sockfd, buffer, strlen(buffer), 0);

    // Informer les autres joueurs de la connexion
    snprintf(buffer, sizeof(buffer), GREEN "%s a rejoint le chat.\n" RESET, player->pseudo);
    broadcast_to_all(buffer, player);

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        receive = recv(player->sockfd, buffer, sizeof(buffer), 0);
        if (receive > 0)
        {
            buffer[strcspn(buffer, "\r\n")] = 0; // Enlever le retour à la ligne

            if (buffer[0] == '/')
            {
                handle_command(player, buffer);
            }
            else
            {
                snprintf(buffer, sizeof(buffer), RED "Commande non reconnue. Tapez /help pour voir la liste des commandes.\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
            }
        }
        else
        {
            // Le joueur s'est déconnecté
            handle_player_disconnect(player);
            pthread_exit(NULL);
        }
    }
}

// Thread principal qui gère la connexion et l'enregistrement des clients
int main()
{
    int server_sockfd, new_sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

    // Création du socket serveur
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0)
    {
        perror("Erreur de création du socket");
        exit(EXIT_FAILURE);
    }

    // Forcer la réutilisation de l'adresse
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // Liaison du socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Erreur de liaison");
        exit(EXIT_FAILURE);
    }

    // Écoute
    if (listen(server_sockfd, 10) < 0)
    {
        perror("Erreur d'écoute");
        exit(EXIT_FAILURE);
    }

    printf("Serveur en attente de joueurs sur le port 8080...\n");

    // Charger les utilisateurs
    load_users();

    socklen_t clilen = sizeof(client_addr);

    while (1)
    {
        new_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);
        if (new_sockfd < 0)
        {
            perror("Erreur d'acceptation");
            continue;
        }

        player_t *player = (player_t *)malloc(sizeof(player_t));
        player->sockfd = new_sockfd;
        player->connected = 1;
        player->game_count = 0;
        player->challenge_sent = 0;
        player->challenge_received = 0;
        player->challenger = NULL;
        player->challengee = NULL;
        player->wins = 0;
        player->losses = 0;
        player->draws = 0;
        pthread_mutex_init(&player->player_mutex, NULL);

        // Recevoir le pseudo
        recv(player->sockfd, player->pseudo, 32, 0);
        player->pseudo[strcspn(player->pseudo, "\r\n")] = 0; // Enlever le retour à la ligne

        printf("Tentative de connexion pour le pseudo : %s\n", player->pseudo);

        // Vérifier si le pseudo existe déjà dans le fichier des utilisateurs
        int user_index = find_user_index(player->pseudo);

        if (user_index == -1)
        {
            // Pseudo inconnu, inviter l'utilisateur à s'enregistrer
            snprintf(buffer, sizeof(buffer), "Bienvenue %s ! Veuillez vous enregistrer.\nEntrez un mot de passe : ", player->pseudo);
            send(player->sockfd, buffer, strlen(buffer), 0);

            // Recevoir le mot de passe
            char password[128];
            recv(player->sockfd, password, sizeof(password), 0);
            password[strcspn(password, "\r\n")] = 0; // Enlever le retour à la ligne

            // Demander la confirmation du mot de passe
            snprintf(buffer, sizeof(buffer), "Confirmez le mot de passe : ");
            send(player->sockfd, buffer, strlen(buffer), 0);

            char password_confirm[128];
            recv(player->sockfd, password_confirm, sizeof(password_confirm), 0);
            password_confirm[strcspn(password_confirm, "\r\n")] = 0; // Enlever le retour à la ligne

            // Vérifier que les mots de passe correspondent
            if (strcmp(password, password_confirm) != 0)
            {
                snprintf(buffer, sizeof(buffer), RED "Les mots de passe ne correspondent pas. Veuillez réessayer.\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
                close(player->sockfd);
                pthread_mutex_destroy(&player->player_mutex);
                free(player);
                continue;
            }

            // Enregistrer le nouvel utilisateur
            int reg_result = register_user(player->pseudo, password);
            if (reg_result != 0)
            {
                snprintf(buffer, sizeof(buffer), RED "Erreur lors de l'enregistrement de l'utilisateur.\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
                close(player->sockfd);
                pthread_mutex_destroy(&player->player_mutex);
                free(player);
                continue;
            }

            snprintf(buffer, sizeof(buffer), GREEN "Enregistrement réussi ! Vous êtes maintenant connecté.\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }
        else
        {
            // Pseudo connu, demander le mot de passe
            snprintf(buffer, sizeof(buffer), "Pseudo reconnu. Veuillez entrer votre mot de passe : ");
            send(player->sockfd, buffer, strlen(buffer), 0);

            // Recevoir le mot de passe
            char password[128];
            recv(player->sockfd, password, sizeof(password), 0);
            password[strcspn(password, "\r\n")] = 0; // Enlever le retour à la ligne

            // Vérifier le mot de passe
            int auth_result = verify_user_password(player->pseudo, password);
            if (auth_result != 0)
            {
                snprintf(buffer, sizeof(buffer), RED "Mot de passe incorrect. Connexion refusée.\n" RESET);
                send(player->sockfd, buffer, strlen(buffer), 0);
                close(player->sockfd);
                pthread_mutex_destroy(&player->player_mutex);
                free(player);
                continue;
            }

            snprintf(buffer, sizeof(buffer), GREEN "Connexion réussie !\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
        }

        // Vérifier si le pseudo est déjà utilisé en jeu
        int pseudo_used_in_game = 0;
        pthread_mutex_lock(&players_mutex);
        for (int i = 0; i < player_count; ++i)
        {
            if (strcmp(players[i]->pseudo, player->pseudo) == 0)
            {
                if (!players[i]->connected)
                {
                    // Reconnexion du joueur
                    pthread_mutex_lock(&players[i]->player_mutex);

                    // Mettre à jour le socket et l'état du joueur
                    players[i]->sockfd = new_sockfd;
                    players[i]->connected = 1;

                    // Informer le joueur de la reconnexion
                    snprintf(buffer, sizeof(buffer), GREEN "Vous avez été reconnecté avec succès.\n" RESET);
                    send(players[i]->sockfd, buffer, strlen(buffer), 0);

                    pthread_mutex_unlock(&players[i]->player_mutex);

                    printf("Joueur %s reconnecté.\n", player->pseudo);
                    free(player);

                    // Relancer le client_handler pour le joueur reconnecté
                    pthread_create(&players[i]->thread, NULL, client_handler, (void *)players[i]);
                    pthread_detach(players[i]->thread);

                    pthread_mutex_unlock(&players_mutex);
                    goto next_client; // Passer au prochain client
                }
                else
                {
                    pseudo_used_in_game = 1;
                    break;
                }
            }
        }

        if (pseudo_used_in_game)
        {
            snprintf(buffer, sizeof(buffer), RED "Ce pseudo est déjà utilisé en jeu. Veuillez réessayer plus tard.\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
            close(player->sockfd);
            pthread_mutex_destroy(&player->player_mutex);
            free(player);
            pthread_mutex_unlock(&players_mutex);
            continue;
        }

        // Ajouter le joueur à la liste
        if (player_count >= MAX_PLAYERS)
        {
            snprintf(buffer, sizeof(buffer), RED "Le serveur est plein. Veuillez réessayer plus tard.\n" RESET);
            send(player->sockfd, buffer, strlen(buffer), 0);
            close(player->sockfd);
            pthread_mutex_destroy(&player->player_mutex);
            free(player);
            pthread_mutex_unlock(&players_mutex);
            continue;
        }

        players[player_count++] = player;
        pthread_mutex_unlock(&players_mutex);

        // Créer un thread pour gérer ce client
        pthread_create(&player->thread, NULL, client_handler, (void *)player);
        pthread_detach(player->thread);

    next_client:
        continue;
    }

    close(server_sockfd);
    return 0;
}
