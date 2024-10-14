// client.c

#include "client.h"

// Fonction pour recevoir les messages du serveur
void *receiver_thread(void *args) {
    receiver_args_t *r_args = (receiver_args_t *)args;
    int sockfd = r_args->sockfd;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        } else if (bytes_received == 0) {
            printf("\nDéconnecté du serveur.\n");
            fflush(stdout);
            exit(EXIT_SUCCESS);
        } else {
            perror("Erreur lors de la réception des données");
            exit(EXIT_FAILURE);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    pthread_t recv_thread;
    receiver_args_t r_args;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <Adresse_IP_Serveur> <Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // Création du socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Erreur de création du socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    // Conversion de l'adresse IP
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Adresse IP invalide");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Connexion au serveur
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur de connexion au serveur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connecté au serveur %s:%d\n", server_ip, server_port);

    // Recevoir les instructions initiales du serveur (login/singup)
    // Le client doit envoyer le pseudo en premier
    printf("Entrez votre pseudo : ");
    fflush(stdout);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        printf("Erreur de lecture du pseudo.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // Envoyer le pseudo au serveur
    send(sockfd, buffer, strlen(buffer), 0);

    // Initialiser les arguments pour le thread récepteur
    r_args.sockfd = sockfd;
    if (pthread_create(&recv_thread, NULL, receiver_thread, (void *)&r_args) != 0) {
        perror("Erreur lors de la création du thread récepteur");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Boucle principale pour envoyer des commandes au serveur
    while (1) {
        // Lire l'entrée utilisateur
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("\nFermeture du client.\n");
            break;
        }

        // Envoyer la commande au serveur
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            perror("Erreur lors de l'envoi des données");
            break;
        }

        // Si l'utilisateur souhaite quitter, fermer le socket et terminer
        if (strncmp(buffer, "/quit", 5) == 0) {
            printf("Déconnexion...\n");
            break;
        }
    }

    // Fermer le socket
    close(sockfd);
    return 0;
}
