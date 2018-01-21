#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"
#include "parser.h"

#define MAX_NUM_MEMBERS 16

void subprocess(int, char *);
void listening_server(int, int[2]);
void mainserver(int[2]);
void handle_main_command(char *, char (*)[]);
//void handle_groupchat_command(char *, char **);
int GPORT = PORT;


struct chatroom {
  char *name;  
  int num_members;
  int port;
  int server_sd;
  char *members[MAX_NUM_MEMBERS];
};

int chatrooms_index = 0;
struct chatroom existing_chatrooms[32];


static void sighandler(int signo){
  switch(signo) {
  case SIGINT:
    printf("\nProcess ended due to SIGINT\n");
    exit(0);
  case SIGUSR1:
    printf("Parent PID: %d\n", getppid());
    break;
  }
}


void print_error(){
  printf("Error: %s\n", strerror(errno));
  exit(0);
}


int main() {
  signal(SIGINT, sighandler);
  signal(SIGUSR1, sighandler);

  int global_listen_socket;
  if((global_listen_socket = server_setup(GPORT++)) != -1){
    printf("[MAIN %d]: Main server successfully created!\n", getpid());
  } else {
    print_error();
  }

  printf("global_listen_socket: %d\n", global_listen_socket);
  
  int pipe_to_main[2];
  pipe2(pipe_to_main, O_NONBLOCK);

  int f = fork();

  if(f == 0){
    sleep(1);

    /* Child */
    mainserver(pipe_to_main);
    exit(0);
  } else {  
    listening_server(global_listen_socket, pipe_to_main);
  }
  
  /*

    printf("global_listen_socket = %d\n", global_listen_socket);  
    char main_buffer[BUFFER_SIZE];
    int global_client_socket, gcs2;
    printf("global_listen_socket = %d\n", global_listen_socket);
    while (read(global_client_socket, main_buffer, sizeof(main_buffer))) {    
    printf("[MAIN %d]: received [%s]\n", getpid(), main_buffer);      
    char *to_write;
    handle_main_command(main_buffer, &to_write);
    write(global_client_socket, to_write, sizeof(main_buffer));
    }  
    close(global_client_socket);
  */
}


/* Listens for incoming connections from other clients */
void listening_server(int global_listen_socket, int pipe_to_main[2]){
  int global_client_socket;

  close(pipe_to_main[0]);
  while(1){
    if((global_client_socket = server_connect(global_listen_socket)) != -1){
      printf("[MAIN %d]: Sockets connected!\n", getpid());
    } else {
      print_error();
      exit(0);
    }
    
    write(pipe_to_main[1], &global_client_socket, sizeof(int));
  }
}


/* Forks off subprocess to deal w/ client */
void mainserver(int pipe_to_main[2]){
  close(pipe_to_main[1]);

  while(1){
    int global_client_socket;
    int r = read(pipe_to_main[0], &global_client_socket, 4);

    // Fork off subprocess to talk to client
    if(r > 0){
      
      if(fork() == 0) {	
	subprocess(global_client_socket, "test");
	exit(0);
      } else {
	// else 
      }
    }
  }
}


void subprocess(int socket, char *group_name) {
  char buffer[BUFFER_SIZE];
  
  while ((read(socket, buffer, sizeof(buffer)) > 0)) {
    printf("[SUB %d for %s]: received [%s]\n", getpid(), group_name, buffer);
    
    char write_to_client[BUFFER_SIZE];
    handle_main_command(buffer, &write_to_client);
    write(socket, write_to_client, sizeof(buffer)); // SIZEOF(BUFFER) ??????
  }
  
  close(socket);
  exit(0);
}


/* 
   subserver method that handles commands from a client in a groupchat
*/
/*
  void handle_groupchat_command(char *s, char **to_client){
  if(strcmp(s, "do") == 0) {
    
  *to_client = "success";
  
  } else if(strcmp(s, "quit") == 0) {
    
  // connect to GLOBAL automatically
  *to_client = "success"; // SEND ID BACK!!!
    
  } else if (strcmp(s, "members") == 0) {
    
  /* List ALL created SUBGROUPS */
/*
 *to_client = "success";
    
 } else {
    
 *to_client = "invalid command";
 }
 }
*/



/* 
   Handles command provided by string s.
   Sets to_client to a return string, which should be
   sent back to the client 


   WILL NEED TO BE PARSED EVENTUALLY!

*/
void handle_main_command(char *s, char (*to_client)[]) {
  int num_phrases = get_num_phrases(s, ' '); // don't move this line
  char **parsed = parse_input(s, " ");

  printf("Num phrases: %d\n", num_phrases);

  /* IMPORTANT 

     THREE TYPES OF "DATA" TO BE SENT BACK TO CLIENT

     Prefixed by: 
     #c : Command (Handled case by case on client)
     #t : Text
     #o : Other (Handled case by case on client)


     COMMAND FORMAT: #c|COMMAND|ADDITIONAL INFO#
     - - - - - - - - - - - - - - - - - -
     #c|join|10002
     #c|do stuff


     TEXT FORMAT: #t|SENDER|TEXT#
     - - - - - - - - - - - - - - - - - - 
     #t|joeuser|this is a public message#
     #t|bobuser|this is a private message!#

     OTHER FORMAT:
     Whatever you want...
  */
  
  if(num_phrases == 1) {
    
    /* Single phrase commmands */    
    if(strcmp(parsed[0], "@list") == 0) {

      /* Update this !!! */      
      strcpy(*to_client, "#t|SERVER|chatroom1: 2 people\nchatroom2: 3 people#");
    } else if(strcmp(parsed[0], "@help") == 0){
      strcpy(*to_client, "#c|display-help#");
    } else if (!strcmp(parsed[0], "@end")) {
      exit(0);
    } else {
      strcpy(*to_client, "#c|display-invalid#");
    }
    
  } else if (num_phrases == 2) {
    char *cr_name;
    struct chatroom curr_cr;
    
    /* Double phrase commands */
    if(strcmp(parsed[0], "@join") == 0) {      
      char pass = 0;
      cr_name = parsed[1];
      
      int i;
      for(i=0; i<sizeof(existing_chatrooms)/sizeof(existing_chatrooms[0]); i++){
	curr_cr = existing_chatrooms[i];
	if(strcmp(curr_cr.name, cr_name) == 0) {
	  sprintf(*to_client, "#c|join|%s#", int_to_str(curr_cr.port));
	  pass = 1;
	}	
      }
      // SHOULD RETURN PORT OF RESPECTIVE CHATROOM

      if(!pass){
	strcpy(*to_client, "#o|chatroom-noexist#");
      }
      
    } else if (strcmp(parsed[0], "@create") == 0) {
      
      /* Make sure chatroom doesn't already exist! */
      cr_name = parsed[1];
      int i;
      char nametaken = 0;
      for(i=0; i<sizeof(existing_chatrooms)/sizeof(existing_chatrooms[0]); i++){
	curr_cr = existing_chatrooms[i];
	if(strcmp(curr_cr.name, cr_name) == 0)
	  nametaken = 1;
      }

      if(nametaken)
	strcpy(*to_client, "#o|chatroom-nametaken#");
      else {    
	char *cr_name = parsed[1];

	struct chatroom chatrm;
	chatrm.name = cr_name;
      
	existing_chatrooms[chatrooms_index] = chatrm;
	chatrooms_index++;
      

	// FORK SUBSERVER
	int lis_sock = server_setup(GPORT++);
	//printf("lis_sock = %d\n", lis_sock);

	printf("[MAIN %d]: chatroom %s created on port %s\n", getpid(), cr_name, int_to_str(GPORT-1));
    
	int f = fork();
	//printf("fork process = %d\n", f);    
    
	if (f == 0) {
	  int client_sock = server_connect(lis_sock);
	  //printf("client_sock = %d\n", client_sock);
      
	  if (client_sock != -1) {
	    subprocess(client_sock, cr_name);

	    printf("[MAIN %d]: Chatroom on port %s closed!\n", getpid(), int_to_str(GPORT-1));
	  } else {
	    printf("[MAIN %d]: Failed to create chatroom!\n", getpid());
	  }
	
	} else {
	  strcpy(*to_client, "#o|chatroom-success#");
	}
      }

      
    } else {

      /* Arguments from client make no sense */
      strcpy(*to_client, "#c|display-invalid#");
    }
	     
  }

  free(parsed);
}
