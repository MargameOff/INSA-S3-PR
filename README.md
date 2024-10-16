# Projet Réseaux - Awale

Fait par Marius DELEUIL & Chloé BUTTIGIEG, 4IFA, 2024.  
Langage C.

## Compiler le projet
Le projet utilise le compilateur GCC. Pour le compiler il faut se placer à la racine du projet et lancer la commande suivante :
```sh
$ make
```

## Lancer le projet

### Lancer le serveur
Pour lancer le serveur, il faut se placer à la racine du projet et lancer la commande :
```sh
./Serveur/serveur
```
Par défaut, le port utilisé est 8080. Pour le modifier, il faut modifier la valeur de la constante ```PORT``` du fichier ```serveur.h```

### Lancer le client
Pour lancer le client, il faut se placer à la racine du projet et lancer la commande :
```sh
./Client/client <adresse_ip> <port>
```
## Commandes client disponibles

- **/defier \<pseudo\>** : Défier un joueur
- **/accepter** : Accepter un défi
- **/refuser** : Refuser un défi
- **/joueurs** : Lister les joueurs connectés
- **/global \<message\>** : Envoyer un message au chat global
- **/mp \<pseudo\> \<message\>** : Envoyer un message privé
- **/chat \<numéro de partie\> \<message\>** : Envoyer un message dans une partie
- **/play \<numéro de partie\> [\<nombre de 0 à 5\>]** : Afficher le plateau ou jouer dans une partie
- **/abandon \<numéro de partie\>** : Abandonner la partie
- **/quit** : Quitter le jeu
- **/help** : Afficher l'aide

