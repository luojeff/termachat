#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
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
#define BUFFER_SIZE 256
#define PORT 10001 // default port attempts start at
#define TEST_IP "127.0.0.1"
#define READ 0
#define WRITE 1

void error_check(int, char *);
int server_setup(int);
int server_connect(int);
int client_setup(char *, char *);

#endif
