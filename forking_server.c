#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"
#include "parser.h"

void subprocess(int, char *, char*);
void listening_server(int, int[2]);
void mainserver(int[2]);
int handle_main_command(char *, char (*)[], char (*)[]);
void print_error();
void intHandler(int);
//void handle_groupchat_command(char *, char **);

static volatile int cont = 1;

int GPORT = PORT;

union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
  struct seminfo *__buf;
};

/* Storing information about chatrooms */
struct chatroom {
  char *name;
  int num_members;
  int port;
  int server_sd;
  char *fifo_to_main;
  
  char *members[MAX_NUM_MEMBERS];
};

#define SEM_KEY 12345
int chatrooms_added = 0;
struct chatroom existing_chatrooms[MAX_NUM_CHATROOMS];

// Creates and decrements a semaphore
int create_semaphore(int sem_key){
  struct sembuf sbuf = {0, -1, SEM_UNDO};
  union semun su;  
  su.val = 1;
  
  int sem_id;  
  if((sem_id = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0644)) < 0) {
    print_error();
  }
  else
    semctl(sem_id, 0, SETVAL, su);
  if((semop(sem_id, &sbuf, 1)) < 0) {
    print_error();
  }

  return sem_id;
}


int get_semaphore(int sem_key){
  int sem_id;
  if((sem_id = semget(sem_key, 0, 0644)) < 0) {
    printf("error: [%d]\n", errno);
    print_error();
  }
  return sem_id;
}


void wait_semaphore(int sem_id){
  struct sembuf sbuf = {0, -1, SEM_UNDO};
  if((semop(sem_id, &sbuf, 1)) < 0) {
    print_error();    
  }
}

void free_semaphore(int sem_id){
  struct sembuf sbuf = {0, 1, SEM_UNDO};
  if((semop(sem_id, &sbuf, 1)) < 0)
    print_error();
}


// Attempt to remove semaphore. Will wait until semaphore is free
// before deleting
int remove_semaphore(int sem_id){
  struct sembuf sbuf = {0, 1, SEM_UNDO};
  
  // Wait until semaphore freed and delete semaphore
  // Remove semaphore
  if((semop(sem_id, &sbuf, 1)) < 0)
    print_error();
  if(semctl(sem_id, 1, IPC_RMID) < 0)
    print_error();
  else
    printf("Semaphore removed\n");
}


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

void print_error(){
  printf("Error from P%d: %s\n", getpid(), strerror(errno));
  exit(0);
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
    
    /* Child */
    mainserver(pipe_to_main);
    exit(0);
  } else if (f > 0) {
    listening_server(global_listen_socket, pipe_to_main);
    exit(0);
  } else {
    print_error();
  }
}


/* Listens for incoming connections from other clients */
void listening_server(int global_listen_socket, int pipe_to_main[2]){
  printf("LISTENSERVER PID: %d\n", getpid());
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
    if(fork() == 0) {
      subprocess(global_client_socket, "pub-test", fifo_name);
      exit(0);
    }
    
    write(pipe_to_main[WRITE], fifo_name, 20);
  }

  exit(0);
}


void mainserver(int pipe_to_main[2]){
  
  
  printf("MAINSERVER PID: %d\n", getpid());
  close(pipe_to_main[WRITE]);
  
  int fd, sem_id = 0;
  int count = 0;
  int server_fds[MAX_CLIENTS];
  int sem_ids[MAX_CLIENTS];
  char fifo_name[20];

  while(cont){
    
    // Handle new client setup stuff
    if(read(pipe_to_main[READ], fifo_name, 20) > 0) {
    
      if((fd = open(fifo_name, O_RDONLY | O_NONBLOCK)) > 0) {	
	printf("[MAIN %d]: Connected to fifo [%s]\n", getpid(), fifo_name);
	server_fds[count++] = fd;
	sem_id = create_semaphore(SEM_KEY);
      } else
	printf("[MAIN %d]: Failed to connect to fifo [%s]\n", getpid(), fifo_name);
    }

    char from_sub[BUFFER_SIZE]; 
    int s_fd = server_fds[0];    
    int i;
    
    for(i=0; i<count; i++){      
      if(read(s_fd, from_sub, sizeof(from_sub)) > 0){
	
	// Process what subserver sends
	if(strcmp(from_sub, "sub-wants-list") == 0) {
	  
	  printf("[MAIN happens!]\n");
	  close(s_fd);

	  free_semaphore(sem_id);
	  
	  // Open in write mode
	  s_fd = open(fifo_name, O_WRONLY);	
	  write(s_fd, "test-1", 10);
	  printf("[MAIN has written!]\n");
	  close(s_fd);	  
	
	  // Reopen in read mode
	  s_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);

	  
	}
      }
    }
    
    // end of while
  }


  // Remove all semaphores !!!
  if(sem_id != 0) {
    remove_semaphore(sem_id);
    sem_id = 0;
  }

  exit(0);
}

void subprocess(int socket, char *group_name, char* fifo_name) {
  char buffer[BUFFER_SIZE];
  int fd;
  
  if((fd = open(fifo_name, O_WRONLY)) > 0)
    printf("[SUB %d for %s]: Connected to fifo [%s]\n", getpid(), group_name, fifo_name);
  else
    printf("[SUB %d for %s]: Error creating FD!\n", getpid(), group_name);

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

      printf("[SUB %d for %s]: Getting semaphore...\n", getpid(), group_name);
      int sem_id = get_semaphore(SEM_KEY);
      printf("[SUB %d for %s]: Attempting to get resource... [%d]\n", getpid(), group_name, sem_id);
      wait_semaphore(sem_id);
      printf("[SUB %d for %s]: Got semaphore resource!\n", getpid(), group_name);
      
      // Open in read mode 
      fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
      while(read(fd, read_from_fifo, BUFFER_SIZE) <= 0);
      printf("[SUB %d for %s]: Received [%s] from MAIN\n", getpid(), group_name, read_from_fifo);

      // Free semaphore
      free_semaphore(sem_id);
      
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
