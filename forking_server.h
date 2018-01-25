#define CONTENTS_SIZE 512

/* Information about chatrooms */
struct chatroom {
  char *name;
  int num_members;
  int port;
  int server_sd;
  char *fifo_to_main;  
  struct node *members;
  char contents[CONTENTS_SIZE][BUFFER_SIZE];
};

struct client {
  int client_pid;
  int chatroom_index;
  int status; /*0: disconnected; 1: connected; 2: joined a room */
};
