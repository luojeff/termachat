#include "networking.h"
#include "helper.h"

void process(char *s);
void subserver(int from_client);
void handle_groupchat_command(char *, char **);
void handle_main_command(char *, char **);

int GPORT = PORT;

static void sighandler(int signo){
  switch(signo) {
  case SIGINT:
    printf("Process ended due to SIGINT\n");
    exit(0);
  case SIGUSR1:
    printf("Parent PID: %d\n", getppid());
    break;
  }
}


int main() {
  signal(SIGINT, sighandler);
  signal(SIGUSR1, sighandler);

  int global_listen_socket = server_setup(GPORT++);
  
  //printf("global_listen_socket = %d\n", global_listen_socket);
  
  char main_buffer[BUFFER_SIZE];
  int global_client_socket = server_connect(global_listen_socket);
  //printf("global_client_socket = %d\n", global_client_socket);
  
  while (read(global_client_socket, main_buffer, sizeof(main_buffer))) {
    
    printf("[MAIN %d]: received [%s]\n", getpid(), main_buffer);

    // Ends server completely
    if (!strcmp(main_buffer, "end")) {
      break;
    }
      
    char *to_write;
    handle_main_command(main_buffer, &to_write);
    write(global_client_socket, to_write, sizeof(main_buffer));
  }
  
  close(global_client_socket);
}

void subserver(int socket) {
  char buffer[BUFFER_SIZE];
  //  printf("[SUB %d]: Get in\n", getpid());

  
  while (read(socket, buffer, sizeof(buffer))) {
    printf("[SUB %d]: received [%s]\n", getpid(), buffer);
    //process(buffer);
    
    char *to_write;
    handle_groupchat_command(buffer, &to_write);
    write(socket, to_write, sizeof(buffer));
  }
  
  close(socket);
  exit(0);
}

/* 
subserver method that handles commands from a client in a groupchat


*/
void handle_groupchat_command(char *s, char **to_client){
  if(strcmp(s, "do") == 0) {
    
    *to_client = "success";
  
  } else if(strcmp(s, "quit") == 0) {
    
    // connect to GLOBAL automatically
    *to_client = "success"; // SEND ID BACK!!!
    
  } else if (strcmp(s, "members") == 0) {
    
    /* List ALL created SUBGROUPS */
    *to_client = "success";
    
  } else {
    
    *to_client = "invalid command";
  }
}


/* 
   Handles command provided by string s.
   Sets to_client to a return string, which should be
   sent back to the client 


   WILL NEED TO BE PARSED EVENTUALLY!

*/
void handle_main_command(char *s, char **to_client) {
  if(strcmp(s, "list") == 0) {
    /* List ALL created CHATROOMS */
    
    *to_client = "chatroom1: 2 people\nchatroom2: 3 people";
  
  } else if(strcmp(s, "join chatroom1") == 0) {    
    
    // connect to chat room
    *to_client = "10002"; // SEND PORT BACK!!!
    
  } else if (strcmp(s, "create chatroom2") == 0) {
    /* Make sure chatroom doesn't already exist! */    
    
    // FORK SUBSERVER
    int lis_sock_1 = server_setup(GPORT++);
    //printf("lis_sock_1 = %d\n", lis_sock_1);

    printf("[MAIN %d]: chatroom created on port %s\n", getpid(), int_to_str(GPORT-1));
    
    int f = fork();
    //printf("fork process = %d\n", f);    
    
    if (f == 0) {
      int client_socket1 = server_connect(lis_sock_1);
      //printf("client_socket1 = %d\n", client_socket1);
      
      if (client_socket1 != -1) {
        subserver(client_socket1);

	printf("Chatroom on port %s closed!\n", int_to_str(GPORT-1));
      } else {
	printf("Error: failed to create chatroom!\n");
      }
      
    } else {
      *to_client = "chatroom-success";
    }

    
  } else {
    *to_client = "invalid command";
  }  
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
