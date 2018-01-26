#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#ifndef NETWORKING_H
#define NETWORKING_H

#define ATTEMPTS 50
#define BUFFER_SIZE 1024
#define PORT 10001 // default port attempts start at
#define TEST_IP "127.0.0.1"

/* Chatroom limitations */
#define MAX_NUM_MEMBERS 16
#define MAX_NUM_CHATROOMS 16
#define MAX_CLIENTS ((MAX_NUM_MEMBERS) * (MAX_NUM_CHATROOMS + 1))
#define MAX_GROUPNAME_SIZE 32
#define MAX_USERNAME_LENGTH 32

#define READ 0
#define WRITE 1

void error_check(int, char *);
int server_setup(int);
int server_connect(int);
int client_setup(char *, char *);

#endif
