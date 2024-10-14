#ifndef CLIENT_H
#define CLIENT_H

// Librairies
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>

// Constants
#define BUFFER_SIZE 1024

// Structures
typedef struct {
    int sockfd;
} receiver_args_t;

#endif