#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"
#include "parser.h"
#include "semaphore.h"

void subprocess(int, char *, char*);
void listening_server(int, int[2]);
void mainserver(int[2]);
void handle_sub_command(char *, char (*)[]);
int handle_main_command(char *, char (*)[], char (*)[]);
void print_error();
void intHandler(int);
//void handle_groupchat_command(char *, char **);

static volatile int cont = 1;

int GPORT = PORT;

int chatrooms_added = 0;
struct chatroom existing_chatrooms[MAX_NUM_CHATROOMS];  

// KEPT FOR TESTING PURPOSES
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


void intHandler(int sig){
  cont = 0;
}


int main() {
  struct sigaction act;
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);
  //signal(SIGINT, sighandler);
  //signal(SIGUSR1, sighandler);
  
  int global_listen_socket;
  if((global_listen_socket = server_setup(GPORT++)) != -1){
    printf("[MAIN %d]: Main server successfully created!\n", getpid());
  } else {
    print_error();
  }
  
  // Unnamed pipe between mainserver and listeningserver
  int pipe_to_main[2];
  pipe2(pipe_to_main, O_NONBLOCK);
  
  // To be implemented...
  char *fifos[MAX_NUM_CHATROOMS];
  
  int f = fork();
  if(f == 0) {
    mainserver(pipe_to_main);
  } else if (f > 0) {
    listening_server(global_listen_socket, pipe_to_main);
  } else {
    print_error();
  }
}


/* Listens for incoming connections from other clients */
void listening_server(int global_listen_socket, int pipe_to_main[2]){
  int global_client_socket;

  close(pipe_to_main[0]);
  while(cont){
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
    int child_pid = fork();
    if(child_pid == 0) {
      subprocess(global_client_socket, "pub-test", fifo_name);
      exit(0);
    }
    
    write(pipe_to_main[WRITE], fifo_name, 20);
    write(pipe_to_main[WRITE], &child_pid, sizeof(child_pid));
  }

  exit(0);
}


void mainserver(int pipe_to_main[2]){
  close(pipe_to_main[WRITE]);
  
  int fd;
  int count = 0;
  int server_fds[MAX_CLIENTS];
  int sem_ids[MAX_CLIENTS];
  char fifo_name[20];

  while(cont){
    
    // Handle new client setup stuff
    if(read(pipe_to_main[READ], fifo_name, 20) > 0) {
    
      if((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) > 0) {	
	printf("[MAIN %d]: Connected to fifo [%s]\n", getpid(), fifo_name);
	server_fds[count] = fd;

	int sub_pid = 0;
	while(read(pipe_to_main[READ], &sub_pid, 4) <= 0);
	
	sem_ids[count] = create_semaphore(sub_pid);
	printf("[MAIN %d]: Semaphore {sem_id: %d} created!\n", getpid(), sem_ids[count]);
	count++;
      } else
	printf("[MAIN %d]: Failed to connect to fifo [%s]\n", getpid(), fifo_name);
    }
    
    int s_fd;
    
    int i;
    
    for(i=0; i<count; i++){
      char from_sub[BUFFER_SIZE];
      s_fd = server_fds[i];
      int sem_id = sem_ids[i];
      int n;

      // Verify that semaphore is used (before sub writes)
      if(is_used(sem_id) && (n = read(s_fd, from_sub, sizeof(from_sub))) > 0){
	
	char to_sub[BUFFER_SIZE];
	printf("[MAIN %d]: Received {%d} bytes from sub: [%s]\n", getpid(), n, from_sub);
	
	handle_sub_command(from_sub, &to_sub);
	
	// Wait for sub to open in read mode before allowing to write
	//printf("[MAIN curr status: %d]\n", is_used(sem_id));
	wait_semaphore(sem_id);
	printf("[MAIN %d]: Got semaphore resource {%d}!\n", getpid(), sem_id);
	
	//close(s_fd);
	//s_fd = open(fifo_name, O_WRONLY);
	write(s_fd, to_sub, sizeof(to_sub));
	printf("[MAIN %d]: Wrote back to sub [%s]\n", getpid(), to_sub);
	//close(s_fd);

	// Reopen in read mode
	printf("now status: [%d]\n", get_sem_val(sem_id));
	free_semaphore(sem_id);
	printf("[MAIN %d]: Freed semaphore resource {%d}!\n", getpid(), sem_id);	
	printf("now status 2: [%d]\n", get_sem_val(sem_id));
	
	
	//s_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
      }
    }
    
    
    // end of while
  }

  int j;
  for(j=0; j<count; j++){
    remove_semaphore(&sem_ids[j]);
  }

  exit(0);
}


void subprocess(int socket, char *group_name, char* fifo_name) {
  char buffer[BUFFER_SIZE];
  int fd, sem_id = 0, sem_id2 = 0;
  
  if((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) > 0) {    
    printf("[SUB %d for %s]: Connected to fifo [%s]\n", getpid(), group_name, fifo_name);
    //close(fd);
  } else
    printf("[SUB %d for %s]: Error creating FD!\n", getpid(), group_name);

  while ((read(socket, buffer, sizeof(buffer)) > 0)) {
    printf("[SUB %d for %s]: Received [%s]\n", getpid(), group_name, buffer);
    
    char write_to_client[BUFFER_SIZE];
    char write_to_fifo[BUFFER_SIZE], read_from_fifo[BUFFER_SIZE];
    
    int resp = handle_main_command(buffer, &write_to_client, &write_to_fifo);
    write(socket, write_to_client, BUFFER_SIZE);

    // Handle writing to mainserver and receiving response
    if(resp > 0) {
      //fd = open(fifo_name, O_WRONLY);
      printf("[SUB %d for %s]: FIFO opened in WRONLY mode!\n", getpid(), group_name);
      
      if(sem_id <= 0) {
	sem_id = get_semaphore(getpid());
	printf("Accessed and received sem_id={%d}\n", sem_id);	
	//printf("[STATUS 1 of {%d}]: [%d]\n", sem_id, is_used(sem_id));
      }

      printf("[SUB %d for %s]: Attempting to get resource {%d}\n", getpid(), group_name, sem_id);
      //printf("[STATUS 2]: {%d}\n", is_used(sem_id));
      if(is_used(sem_id) == 0)
	wait_semaphore(sem_id);
      //printf("[STATUS 3]: {%d}\n", is_used(sem_id));
      printf("[SUB %d for %s]: Got semaphore resource {%d}!\n", getpid(), group_name, sem_id);
      
      write(fd, write_to_fifo, sizeof(write_to_fifo));
      printf("[SUB %d for %s]: Wrote to MAIN [%s]\n", getpid(), group_name, write_to_fifo);
      //close(fd);
      
      // Open in read mode
      // Free to allow mainserver to start writing

      //fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
      printf("[SUB %d for %s]: FIFO %d opened in RDONLY mode!\n", getpid(), group_name, fd);       
      free_semaphore(sem_id);
      printf("[SUB %d for %s]: Freed semaphore resource {%d}\n", getpid(), group_name, sem_id);           


      
      while(read(fd, read_from_fifo, BUFFER_SIZE) <= 0);
      printf("[SUB]: Received [%s]\n", read_from_fifo);
      
      //while((n = read(fd, read_from_fifo, BUFFER_SIZE)) <= 0);
      //printf("[SUB %d for %s]: Received {%d} bytes [%s] from MAIN\n", getpid(), group_name, n, read_from_fifo);

      // Reopen in write mode
      //wait_semaphore(sem_id);
      //printf("[SUB %d for %s]: Got semaphore resource again!\n", getpid(), group_name);
      
      //close(fd);
      //fd = open(fifo_name, O_WRONLY);  
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



void handle_sub_command(char *s, char (*to_sub)[]){
  if(strcmp(s, "sub-wants-list") == 0){
    char to_send[] = "test-1";
    strcpy(*to_sub, to_send);
  } else {
    char to_send[] = "invalid";
    strcpy(*to_sub, to_send);
  }  
}


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
