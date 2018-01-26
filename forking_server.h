#include "networking.h"

#ifndef FORKING_SERVER_H
#define FORKING_SERVER_H

#define CONTENTS_SIZE 512

/* Information about chatrooms */
struct chatroom {
  char *name;
  int num_members;
  int is_valid;
  int server_sd;
  int curr_line;
  struct node *members;
  char contents[CONTENTS_SIZE][BUFFER_SIZE];
};

struct client {
  int client_sub_pid;
  int chatroom_index;
  int status; /*0: disconnected; 1: connected; 2: joined a room */
  char *user_name;
  int pos;  
};

void subprocess(int, char *, char*);
void listening_server(int, int[2]);
void mainserver(int[2]);
void handle_sub_command(char *, char (*)[]);
void handle_info_command(char *, char (*)[]);
int handle_main_command(char *, char (*)[], char (*)[], char *);
void print_error();
void intHandler(int);
int find_client_index(int);
char *print_chatrooms();
size_t get_file_size(const char *);

#endif
