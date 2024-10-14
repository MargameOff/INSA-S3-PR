CC = gcc
CFLAGS = -Wall -pthread

SERVEUR_BIN = Serveur/serveur
CLIENT_BIN = Client/client

all: $(SERVEUR_BIN) $(CLIENT_BIN)

$(SERVEUR_BIN): Serveur/serveur.c Serveur/serveur.h
	$(CC) $(CFLAGS) -o $(SERVEUR_BIN) Serveur/serveur.c

$(CLIENT_BIN): Client/client.c Client/client.h
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) Client/client.c

clean:
	rm -f $(SERVEUR_BIN) $(CLIENT_BIN)