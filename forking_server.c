#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"
#include "parser.h"

void subprocess(int, char *, char*);
void listening_server(int, int[2]);
void mainserver(int[2]);
int handle_main_command(char *, char (*)[], char (*)[]);
//void handle_groupchat_command(char *, char **);

int GPORT = PORT;

/* KEPT FOR TESTING PURPOSES */
void _subprocess(int socket, char *group_name) {  
  char buffer[BUFFER_SIZE];
  while ((read(socket, buffer, sizeof(buffer)) > 0)) {
    printf("[SUB %d for %s]: received [%s]\n", getpid(), group_name, buffer);
    printf("Warning: this subprocess won't write to FIFOs!\n");
    
    char write_to_client[BUFFER_SIZE];
    handle_main_command(buffer, 0, &write_to_client);
    write(socket, write_to_client, sizeof(buffer)); // SIZEOF(BUFFER) ??????
  }  
  close(socket);
  exit(0);
}


/* Storing information about chatrooms */
struct chatroom {
  char *name;
  int num_members;
  int port;
  int server_sd;
  char *fifo_to_main;
  
  char *members[MAX_NUM_MEMBERS];
};

int chatrooms_added = 0;
struct chatroom existing_chatrooms[MAX_NUM_CHATROOMS];


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

  /* Unnamed pipe between mainserver and listeningserver */
  int pipe_to_main[2];
  pipe2(pipe_to_main, O_NONBLOCK);

  /* To be implemented ... */
  char *fifos[MAX_NUM_CHATROOMS];
  
  int f = fork();
  if(f == 0){
    
    /* Child */
    mainserver(pipe_to_main);
    exit(0);
  } else if (f > 0) {
    listening_server(global_listen_socket, pipe_to_main);
  } else {
    print_error();
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
    
    //creating a FIFO to allow interprocess communication
    char fifo_name[20];
    sprintf(fifo_name, "./FIFO%d\0", global_client_socket);
    printf("[MAIN]: Creating fifo [%s]\n", fifo_name);
    mkfifo(fifo_name, 0666);
    printf("[MAIN]: Fifo created!\n");
    
    //fork off new process to talk to client and have it connect to the new FIFO
    if(fork() == 0) {	
      //printf("Subprocess forked off\n");
      subprocess(global_client_socket, "pub-test", fifo_name);
      exit(0);
    }
    
    write(pipe_to_main[WRITE], fifo_name, 20);
  }
}



void mainserver(int pipe_to_main[2]){
  close(pipe_to_main[1]);

  int fd;
  int count = 0;
  int server_fds[(MAX_NUM_CHATROOMS + 1) * MAX_NUM_MEMBERS];
  char fifo_name[20];

  while(1){
    //printf("[MAIN %d]: Received fifo [%s]\n", getpid(), fifo_name);
    
    // Handle new client setup stuff
    if(read(pipe_to_main[READ], fifo_name, 20) > 0) {
      
    
      if((fd = open(fifo_name, O_RDONLY)) > 0) {
	printf("[MAIN %d]: Connected to fifo [%s]\n", getpid(), fifo_name);
	server_fds[count++] = fd;
      } else
	printf("[MAIN %d]: Failed to connect to fifo [%s]\n", getpid(), fifo_name);
    }

    
    // Constantly read from all FIFOs
    int i;
    for(i=0; i<count; i++){
      int s_fd = server_fds[i];
      char from_sub[BUFFER_SIZE];      

      // Read from subprocess fifo
      if(read(s_fd, from_sub, sizeof(from_sub)) > 0) {

	// Process what subserver sends
	if(strcmp(from_sub, "sub-wants-list") == 0) {
	
	  close(s_fd);

	  // Open in write mode
	  s_fd = open(fifo_name, O_WRONLY);	
	  write(s_fd, "test-1", 10);
	  close(s_fd);

	  // Reopen in read mode
	  s_fd = open(fifo_name, O_RDONLY);
	}
      }      
    }
    
  }
  
}


void subprocess(int socket, char *group_name, char* fifo_name) {
  char buffer[BUFFER_SIZE];
  int fd;
  
  if((fd = open(fifo_name, O_WRONLY)) > 0)
    printf("[SUB %d for %s]: Connected to fifo [%s]\n", getpid(), group_name, fifo_name);

  while ((read(socket, buffer, sizeof(buffer)) > 0)) {
    printf("[SUB %d for %s]: Received [%s]\n", getpid(), group_name, buffer);
    
    char write_to_client[BUFFER_SIZE];
    char write_to_fifo[BUFFER_SIZE], read_from_fifo[BUFFER_SIZE];
    
    int resp = handle_main_command(buffer, &write_to_client, &write_to_fifo);
    write(socket, write_to_client, BUFFER_SIZE);
    
    // Handle writing to mainserver and receiving response
    if(resp > 0) {
      write(fd, write_to_fifo, BUFFER_SIZE);
      printf("[SUB %d for %s]: Wrote to MAIN [%s]\n", getpid(), group_name, write_to_fifo);
      close(fd);

      // Open in read mode 
      fd = open(fifo_name, O_RDONLY);
      read(fd, read_from_fifo, BUFFER_SIZE);
      printf("[SUB %d for %s]: Received [%s] from MAIN\n", getpid(), group_name, read_from_fifo);
      close(fd);

      // Reopen in write mode
      fd = open(fifo_name, O_WRONLY);
    }
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
   s: string with command arguments that need to be handled
   fifo_fd: descriptor of fifo between MAIN and SUB
   to_client: pointer to array that will be sent back to client. Set accordingly by
   what the client inputs.

   RETURNS 1 IF THE SUBPROCESS NEEDS TO TALK TO THE MAINSERVER IN ADDITION TO THE CLIENT

   RETURNS 0 IF THE SUBPROCESS ONLY NEEDS TO TALK TO CLIENT

*/
int handle_main_command(char *s, char (*to_client)[], char (*to_fifo)[]) {
  int num_phrases = get_num_phrases(s, ' '); // don't move this line
  char **parsed = parse_input(s, " ");

  /* IMPORTANT 

     THREE "KINDS" OF DAT A TO BE SENT FROM *SUBPROCESS* to *CLIENT*

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


     COMMANDS that involve chatrooms must send
     data back to the MAINSERVER by writing to the FIFO. 
  */
  
  if(num_phrases == 1) {
    
    /* Single phrase commmands */    
    if(strcmp(parsed[0], "@list") == 0) {
      /* Update this !!! */      
      //strcpy(*to_client, "#t|SERVER|chatroom1: 2 people\nchatroom2: 3 people#");

      strcpy(*to_client, "#o|requesting#");
      strcpy(*to_fifo, "sub-wants-list");
      return 1;      
    } else if(strcmp(parsed[0], "@help") == 0){
      strcpy(*to_client, "#c|display-help#");
      return 0;
    } else if (!strcmp(parsed[0], "@end")) {
      return 0;
    } else {
      strcpy(*to_client, "#c|display-invalid#");
      return 0;
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

      return 1;
    } else if (strcmp(parsed[0], "@create") == 0) {
      
      /* Make sure chatroom doesn't already exist! */
      cr_name = parsed[1];
      char nametaken = 0;
      int i;
      
      for(i=0; i<chatrooms_added; i++){	
	curr_cr = existing_chatrooms[i];
	if(strcmp(curr_cr.name, cr_name) == 0) {
	  nametaken = 1;
	  break;
	}
      }

      printf("Groupchat nametaken: [%d]\n", nametaken);

      if(nametaken)
	strcpy(*to_client, "#o|chatroom-nametaken#");
      else {    
	char *cr_name = parsed[1];
	int lis_sock = server_setup(GPORT++);
	//printf("lis_sock = %d\n", lis_sock);	

	printf("[MAIN %d]: chatroom %s created on port %s\n", getpid(), cr_name, int_to_str(GPORT-1));	

	struct chatroom chatrm;
	chatrm.name = cr_name;      
	existing_chatrooms[chatrooms_added] = chatrm;
	chatrooms_added++;
	

	/* Find a way to redo this code */
	int f = fork();
    
	if (f == 0) {
	  int client_sock = server_connect(lis_sock);
	  printf("client_sock = %d\n", client_sock);
      
	  if (client_sock != -1) {
	    _subprocess(client_sock, cr_name);

	    printf("[MAIN %d]: Chatroom on port %s closed!\n", getpid(), int_to_str(GPORT-1));
	  } else {
	    printf("[MAIN %d]: Failed to create chatroom!\n", getpid());
	  }	
	}  else {
	  strcpy(*to_client, "#o|chatroom-success#");
	}
      }

      return 1;
      
    } else {
      
      strcpy(*to_client, "#c|display-invalid#");
      return 0;
    }
	     
  } else {    
    strcpy(*to_client, "#c|display-invalid#");
  }

  free(parsed);
}
