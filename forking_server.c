#include "networking.h"

void process(char *s);
void subserver(int from_client);
void handle_command(char *, char **);

int main() {

  int listen_socket;
  int f;
  listen_socket = server_setup();

  while (1) {

    int client_socket = server_connect(listen_socket);
    f = fork();
    if (f == 0)
      subserver(client_socket);
    else
      close(client_socket);
  }
}

void subserver(int client_socket) {
  char buffer[BUFFER_SIZE];

  while (read(client_socket, buffer, sizeof(buffer))) {
    printf("[SUB %d] received: [%s]\n", getpid(), buffer);
    //process(buffer);

    char *to_write;   

    handle_command(buffer, &to_write);
    write(client_socket, to_write, sizeof(buffer));
  }
  
  close(client_socket);
  exit(0);
}

/* Creates group chat server */
void group_chat(){
}

/* subserver method that handles command from client.
CURRENTLY SUPPORTED:
'do' -- for testing
'connect <id>'
 */

void handle_command(char *s, char **to_client){
  if(strcmp(s, "do") == 0)
    *to_client = "success";
  else if(strcmp(s, "connect") == 0) {
    // connect to GLOBAL automatically
    *to_client = "success"; // SEND ID BACK!!!
    
  } else if (strcmp(s, "list") == 0){
    /* List ALL created SUBGROUPS */
    *to_client = "success";
  } else
    *to_client = "fail";
}

/*
void process(char * s) {
  while (*s) {
    if (*s >= 'a' && *s <= 'z')
      *s = ((*s - 'a') + 13) % 26 + 'a';
    else  if (*s >= 'A' && *s <= 'Z')
      *s = ((*s - 'a') + 13) % 26 + 'a';
    s++;
  }
}
*/
