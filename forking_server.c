#define _GNU_SOURCE
#include "list.h"
#include "networking.h"
#include "helper.h"
#include "parser.h"
#include "semaphore.h"
#include "forking_server.h"

static volatile int cont = 1;
int repeat = 1;

int GPORT = PORT;

int chatrooms_added = 0;
int num_clients = 0;
struct chatroom existing_chatrooms[MAX_NUM_CHATROOMS];
struct client clients[MAX_CLIENTS];


void intHandler(int sig){
  cont = 0;
}


int main() {
  struct sigaction act;
  act.sa_handler = intHandler;
  sigaction(SIGINT, &act, NULL);
  
  
  int global_listen_socket;
  if((global_listen_socket = server_setup(GPORT++)) != -1)
    printf("[MAIN %d]: Main server successfully created!\n", getpid());
  else
    print_error();
  
  
  // Unnamed pipe between mainserver and listeningserver
  int pipe_to_main[2];
  pipe2(pipe_to_main, O_NONBLOCK);
  
  int f = fork();
  if(f == 0)
    mainserver(pipe_to_main);
  else if (f > 0)
    listening_server(global_listen_socket, pipe_to_main);
  else 
    print_error();
}


// Listens for incoming connections from other clients
void listening_server(int global_listen_socket, int pipe_to_main[2]){
  
  int global_client_socket;
  close(pipe_to_main[0]);
  
  while(cont){
    
    if((global_client_socket = server_connect(global_listen_socket)) != -1){
      printf("[MAIN %d]: Sockets connected!\n", getpid());
    
      //creating a FIFO to allow interprocess communication
      char fifo_name[20];
      snprintf(fifo_name, sizeof(fifo_name), "./FIFO%d", global_client_socket);
      printf("[MAIN]: Creating fifo [%s]\n", fifo_name);
      mkfifo(fifo_name, 0666);
      printf("[MAIN]: Fifo created!\n");
    
      //fork off new process to talk to client and have it connect to the new FIFO
      int child_pid = fork();
      if(child_pid == 0) {
        subprocess(global_client_socket, "USER", fifo_name);
        exit(0);
      }
    
      write(pipe_to_main[WRITE], fifo_name, 20);
      write(pipe_to_main[WRITE], &child_pid, sizeof(child_pid));
    }
  } 

	
  exit(0);
}


void mainserver(int pipe_to_main[2]){
  close(pipe_to_main[WRITE]);
  
  int fd, i;
  int sub_count = 0; // # of subprocesses connected
  int server_fds[MAX_CLIENTS], sem_ids[MAX_CLIENTS];
  char fifo_name[20];
  char from_sub[BUFFER_SIZE], to_sub[BUFFER_SIZE];

  while(cont){
    
    // Handle new client setup stuff
    if(read(pipe_to_main[READ], fifo_name, 20) > 0) {
    
      if((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) > 0) {	
	printf("[MAIN %d]: Connected to fifo [%s]\n", getpid(), fifo_name);
	server_fds[sub_count] = fd;

	int sub_pid = 0;
	while(read(pipe_to_main[READ], &sub_pid, 4) <= 0);
	
	sem_ids[sub_count] = create_semaphore(sub_pid);
	//printf("[MAIN %d]: Semaphore {sem_id: %d} created!\n", getpid(), sem_ids[sub_count]);
	sub_count++;

	//printf("DEBUG:: Add a new client into table: %d\n", sub_pid);
        struct client new_client;
        new_client.client_sub_pid = sub_pid;
        new_client.chatroom_index = -1;
        new_client.status = 1;
	new_client.pos = 0;
	new_client.user_name = (char*)malloc(MAX_USERNAME_LENGTH*sizeof(char) + 1);
        clients[num_clients++] = new_client;
      } else
	printf("[MAIN %d]: Failed to connect to fifo [%s]\n", getpid(), fifo_name);
    
    }

    
    int sem_id, sub_fd;
    
    for(i=0; i<sub_count; i++){
      sub_fd = server_fds[i];
      sem_id = sem_ids[i];

      // Verify that semaphore is used (before sub writes)
      if(is_used(sem_id) && (read(sub_fd, from_sub, sizeof(from_sub)) > 0)){
	
	printf("[MAIN %d]: Received from sub [%s]\n", getpid(), from_sub);

	handle_sub_command(from_sub, &to_sub);

	// Wait for subprocess to read before writing
	wait_semaphore(sem_id);	
	write(sub_fd, to_sub, sizeof(to_sub));
	printf("[MAIN %d]: Sent response to subprocess\n", getpid());
	free_semaphore(sem_id);
      }
    }
    
    
    // end of while
  }

  for(i=0; i<sub_count; i++){
    remove_semaphore(&sem_ids[i]);
  }

  exit(0);
}


void subprocess(int socket, char *user, char* fifo_name) {
  char buffer[BUFFER_SIZE], client_name[MAX_USERNAME_LENGTH];
  int fd, sem_id = 0;

  if((fd = open(fifo_name, O_RDWR | O_NONBLOCK)) > 0) {    
    printf("[SUB %d - %s]: Connected to fifo [%s]\n", getpid(), user, fifo_name);
    unlink(fifo_name);
  } else
    printf("[SUB %d - %s]: Error creating FD!\n", getpid(), user);

  // Read client user name
  while(read(socket, client_name, sizeof(client_name)) <= 0)
    user = client_name;
  
  while (repeat) {
    if(read(socket, buffer, sizeof(buffer)) > 0){
      printf("[SUB %d - %s]: Received [%s]\n", getpid(), user, buffer);
    
      char write_to_client[BUFFER_SIZE];
      char write_to_fifo[BUFFER_SIZE], read_from_fifo[BUFFER_SIZE];
    
      int resp = handle_main_command(buffer, &write_to_client, &write_to_fifo, client_name);
      //printf("DEBUG: %s\n", write_to_client);
      write(socket, write_to_client, sizeof(write_to_client));
    
      // Listen to chat response from mainserver?
      // Send that stuff to client    

      // Handle writing to mainserver and receive a response
      if(resp > 0) {
      
        if(sem_id <= 0)
       	  sem_id = get_semaphore(getpid());
      
        if(is_used(sem_id) == 0)
	  wait_semaphore(sem_id);
      
        write(fd, write_to_fifo, sizeof(write_to_fifo));
        printf("[SUB %d - %s]: Wrote to MAIN [%s]\n", getpid(), user, write_to_fifo);      

        // Allow mainserver to write
        free_semaphore(sem_id);
        memset(read_from_fifo, 0, BUFFER_SIZE);
      
        while(read(fd, read_from_fifo, sizeof(read_from_fifo)) <= 0);
        printf("[SUB %d - %s]: Received mainserver response\n", getpid(), user);

        // Process read_from_fifo
        handle_info_command(read_from_fifo, &write_to_client);

        // Write back to client once again
	//printf("DEBUG: %s\n", write_to_client);
        write(socket, write_to_client, sizeof(write_to_client));
      }
    } 
  }
  
  close(socket);
  exit(0);
}


// Handles responses sent from mainserver back to subprocess
void handle_sub_command(char *s, char (*to_sub)[]){
  char invalid_request[] = "invalid-sub-request";
  
  int num_phrases = get_num_phrases(s, '|');
  char **parsed = parse_input(s, "|");
  
  if(num_phrases == 1) {
    if(strcmp(parsed[0], "sub-wants-list") == 0){

      // To be given to client to parse
      char to_send[BUFFER_SIZE] = "#t|SERVER|\n---------- Available chatrooms ----------\n";

      strcat(to_send, print_chatrooms());
      strcat(to_send, "#");
      strcpy(*to_sub, to_send);
      
    }  else {
      strcpy(*to_sub, invalid_request);
    }
    
  } else if (num_phrases == 2) {
    
    if (strcmp(parsed[0], "sub-wants-chat") == 0) {
      char contents[BUFFER_SIZE], filename[32];
      int client_sub_pid = atoi(parsed[1]);
      int client_index = find_client_index(client_sub_pid);
      struct client cl = clients[client_index];
      
      if(cl.status <= 1) {
	strcpy(*to_sub, "#o|not-in-chatroom#");
      } else {
	memset(contents, 0, sizeof(contents));
	struct chatroom cr = existing_chatrooms[cl.chatroom_index];	
	sprintf(filename, "%s.txt", cr.name);

	int fd = open(filename, O_RDONLY | O_CREAT, 0644);
	lseek(fd, cl.pos, SEEK_SET);
	read(fd, contents, sizeof(contents));
	close(fd);
	clients[client_index].pos = get_file_size(filename);	
	sprintf(*to_sub, "#o|read|%s#", contents);
      }
    } else if (strcmp(parsed[0], "sub-wants-history") == 0) {
      char contents[BUFFER_SIZE], filename[32];
      int client_sub_pid = atoi(parsed[1]);
      int client_index = find_client_index(client_sub_pid);
      struct client cl = clients[client_index];      
     
      if(cl.status <= 1) {
	strcpy(*to_sub, "#o|not-in-chatroom#");
      } else {
	memset(contents, 0, sizeof(contents));
	struct chatroom cr = existing_chatrooms[cl.chatroom_index];	
	sprintf(filename, "%s.txt", cr.name);
	
	int fd = open(filename, O_RDONLY | O_CREAT, 0644);

	int file_size = get_file_size(filename);

	int pos;
	if(file_size - BUFFER_SIZE / 2 <= 0){
	  pos = 0;
	} else {
	  pos = file_size - BUFFER_SIZE / 2;
	}
	
	lseek(fd, pos, SEEK_SET);
	read(fd, contents, sizeof(contents));
	close(fd);
	clients[client_index].pos = get_file_size(filename);	
	sprintf(*to_sub, "#o|hread|%s#", contents);
      }
      
    } else if (strcmp(parsed[0], "create") == 0) {

      // Check to see if chatroom name is taken      
      int i;
      char nametaken = 0;      
      for(i=0; i<chatrooms_added; i++){
	
        struct chatroom curr_cr = existing_chatrooms[i];
        if(strcmp(curr_cr.name, parsed[1]) == 0) {
	  nametaken = 1;
          break;
        }
      }

      if(nametaken) {
	
        strcpy(*to_sub, "#o|chatroom-nametaken#");
	printf("[MAIN %d]: Chatroom name [%s] taken!\n", getpid(), parsed[1]);
      } else {
	
        struct chatroom chatrm;
	chatrm.name = (char *)malloc(strlen(parsed[1]) * sizeof(char) + 1);
        strcpy(chatrm.name, parsed[1]);
	chatrm.num_members = 0;
	chatrm.members = NULL;
	chatrm.is_valid = 1;
	//chatrm.contents = (char **)malloc(CONTENTS_SIZE * BUFFER_SIZE);

	
	// Attempt to insert where there is an invalid (deleted chatroom)
	int i;
	int done = 0;
	for(i=0; i<chatrooms_added; i++){
	  if(existing_chatrooms[i].is_valid == 0){
	    existing_chatrooms[i] = chatrm;
	    done = 1;
	    break;
	  }
	}

	// If no invalid chatroom before, insert at the end and increment
	if(!done) {
	  existing_chatrooms[chatrooms_added] = chatrm;
	  chatrooms_added++;
	}

        char chatfile[32];
        sprintf(chatfile, "%s.txt", parsed[1]);
        int fd = open(chatfile, O_CREAT, 0644);
	close(fd);	
        printf("[MAIN %d]: chatroom %s created\n", getpid(), parsed[1]);
        strcpy(*to_sub, "#o|chatroom-created#");
      }
      
    } else if (strcmp(parsed[0], "exit") == 0) {
      
      // Quit chat room
      int client_sub_pid = atoi(parsed[1]);
      int client_index = find_client_index(client_sub_pid);
      
      if (client_index > -1) {
        int chatroom_index = clients[client_index].chatroom_index;
	struct client cl = clients[client_index];
	
	//printf("DEBUG:: chatroom_index = %d\n", clients[client_index].chatroom_index);
        if (chatroom_index > -1) {
          existing_chatrooms[chatroom_index].members = delete_member(existing_chatrooms[chatroom_index].members, cl);
	  existing_chatrooms[chatroom_index].num_members--;
	  clients[client_index].chatroom_index = -1;
	  clients[client_index].status = 0;
        }
      }
      
      strcpy(*to_sub, "#c|exit#");
      
    } else if (strcmp(parsed[0], "delete") == 0) {
      //printf("DEBUG: handle delete room\n");
      int i;
      for(i=0; i<chatrooms_added; i++){
	struct chatroom curr = existing_chatrooms[i];

	if(strcmp(curr.name, parsed[1]) == 0) {
	  if(curr.num_members > 0) {
	    
	    strcpy(*to_sub, "#o|chat-has-members#");
	    break;
	  } else if (curr.num_members == 0) {
	    
	    // Go ahead and delete
	    free(existing_chatrooms[i].name);
	    existing_chatrooms[i].is_valid = 0;
	    char chatfile[32];
	    sprintf(chatfile, "%s.txt", parsed[1]);
	    unlink(chatfile);
	    strcpy(*to_sub, "#o|delete-success#");	    
	    break;
	  }
	}
      }
      //printf("DEBUG:: finish delete room\n");
    } else if (strcmp(parsed[0], "sub-wants-leave") == 0) {
      
      int client_sub_pid = atoi(parsed[1]);
      int client_index = find_client_index(client_sub_pid);
      char *name;
      int pass = 1;
      
      if (client_index > -1) {
        int chatroom_index = clients[client_index].chatroom_index;
	struct client cl = clients[client_index];	

        if (chatroom_index > -1) {
          existing_chatrooms[chatroom_index].members = delete_member(existing_chatrooms[chatroom_index].members, cl);
	  existing_chatrooms[chatroom_index].num_members--;	  
	  name = existing_chatrooms[chatroom_index].name;
	  clients[client_index].chatroom_index = -1;
	  clients[client_index].status = 1;
	  clients[client_index].pos = 0;
        } else {
	  pass = 0;
	}
      }

      if(pass)
	sprintf(*to_sub, "#o|left-chat|%s#", name);
      else
	sprintf(*to_sub, "#o|unable-leave|%s#", name);
    } else {
      strcpy(*to_sub, invalid_request);
    }
    
  } else if (num_phrases >= 3) {

    if(strcmp(parsed[0], "write") == 0){
      int client_sub_pid = atoi(parsed[1]);
      char contents[256], filename[32];
      
      struct client cl = clients[find_client_index(client_sub_pid)];      
      
      if(cl.status <= 1) {
	strcpy(*to_sub, "#o|not-in-chatroom#");
      } else {
	struct chatroom cr = existing_chatrooms[cl.chatroom_index];
	sprintf(filename, "%s.txt", cr.name);
	sprintf(contents, "[%s]: %s\n", cl.user_name, parsed[2]);

	int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
	write(fd, contents, strlen(contents));
	close(fd);
      
	strcpy(*to_sub, "#o|written#");
      }
      
    } else if (strcmp(parsed[0], "join") == 0) {
      
      // Join chat room
      int client_sub_pid = atoi(parsed[2]);
      char *client_name = parsed[3];
      int client_index = find_client_index(client_sub_pid);
      
      if (client_index > -1) {
	if (clients[client_index].chatroom_index == -1) {

	  //struct chatroom curr;

          int i;
	  int pass = 0;
          for(i=0; i<chatrooms_added; i++){
	    //curr = existing_chatrooms[i];
	    
	    if(existing_chatrooms[i].is_valid && (strcmp(existing_chatrooms[i].name, parsed[1]) == 0)) {

	      struct client cl = clients[client_index];
	      
	      // Add into chatroom member
	      existing_chatrooms[i].members = insert_front(existing_chatrooms[i].members, cl);
	      existing_chatrooms[i].num_members++;
	      
	      clients[client_index].chatroom_index = i;
	      clients[client_index].status = 2;	      
	      clients[client_index].user_name = realloc(clients[client_index].user_name, strlen(client_name)*sizeof(char)+1);
	      char chatfile[32];
	      sprintf(chatfile, "%s.txt", parsed[1]);
	      clients[client_index].pos = get_file_size(chatfile);	      
	      strcpy(clients[client_index].user_name, client_name);
	      sprintf(*to_sub, "#c|join|%s#", parsed[1]);
	      pass = 1;
	      break;
	    }	
          }          

          if(!pass){
	    strcpy(*to_sub, "#o|chatroom-noexist#");
          }
	} else {
	  strcpy(*to_sub, "#o|already-joined-other-chatroom#");
	  //existing_chatrooms[clients[client_index].chatroom_index].name);
	}
      } else {
	strcpy(*to_sub, "#o|client-noexist#");
      }	
    } else {
      strcpy(*to_sub, invalid_request);
    }
  } else {
    strcpy(*to_sub, invalid_request);
  }  
}


int find_client_index(int client_sub_pid) {
  int client_index = -1;
  int i;
  for (i=0; i<num_clients; i++) {
    if(clients[i].client_sub_pid == client_sub_pid && clients[i].status > 0) {
      client_index = i;
      break;
    }
  }
  return client_index;
}

char *print_chatrooms() {
  
  char *s = (char *)malloc(200 * sizeof(char));
  int i;
  for(i=0; i<chatrooms_added;i++) {
    struct chatroom chatroom = existing_chatrooms[i];

    if(chatroom.is_valid > 0){
      sprintf(s, "%s%s: ", s, chatroom.name);
    
      if (chatroom.num_members > 0)
	sprintf(s, "%s%s", s, print_list(chatroom.members));    
      sprintf(s, "%s\n", s);
    }
  }
  
  return s;
}

size_t get_file_size(const char *filename) {
  struct stat st;
  if(stat(filename, &st) != 0) {
    return 0;
  }
  return st.st_size;
}

/* 
   Handles information sent back from mainserver to be
   sent back to client 
*/
void handle_info_command(char *s, char (*to_client)[]){
  strcpy(*to_client, s);
}


/* 
   s: string with command arguments that need to be handled
   fifo_fd: descriptor of fifo between MAIN and SUB
   to_client: pointer to array that will be sent back to client. Set accordingly by
   what the client inputs.

   RETURNS 1 IF THE SUBPROCESS NEEDS TO TALK TO THE MAINSERVER IN ADDITION TO THE CLIENT

   RETURNS 0 IF THE SUBPROCESS ONLY NEEDS TO TALK TO CLIENT

*/
int handle_main_command(char *s, char (*to_client)[], char (*to_fifo)[], char *client_name) {
  //printf("DEBUG:: received: %s\n", s);
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
      
      //strcpy(*to_client, "#o|requesting#");
      strcpy(*to_client, "wait");
      strcpy(*to_fifo, "sub-wants-list");
      return 1;
      
    } else if(strcmp(parsed[0], "@help") == 0){
      
      strcpy(*to_client, "#c|display-help#");
      return 0;
      
    } else if (strcmp(parsed[0], "@exit") == 0) {
      
      //strcpy(*to_client, "#c|exit#");
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "exit|%d", getpid());
      repeat = 0;
      return 1;
      
    } else if (strcmp(parsed[0], "@leave") == 0) {
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "sub-wants-leave|%d", getpid());
      
      return 1;
    } else if (strcmp(parsed[0], "@r") == 0) {
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "sub-wants-chat|%d", getpid());
      return 1;
      
    } else if (strcmp(parsed[0], "@history") == 0){
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "sub-wants-history|%d", getpid());
      return 1;
    } else {
      
      strcpy(*to_client, "#c|display-invalid#");
      return 0;
      
    }
    
  } else if (num_phrases >= 2) {
    
    /* Double phrase commands */
    if(strcmp(parsed[0], "@join") == 0) {
      //char pass = 0;
      //cr_name = parsed[1];
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "join|%s|%d|%s", parsed[1], getpid(), client_name);

      return 1;
    } else if (strcmp(parsed[0], "@create") == 0) {
      
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "create|%s", parsed[1]);

      return 1;
      
    } else if (strcmp(parsed[0], "@w") == 0) {
      strcpy(*to_client, "wait");

      // Parse out text
      int i;
      char to_write[BUFFER_SIZE];
      
      strcpy(to_write, parsed[1]);
      if(num_phrases != 2)
	strcat(to_write, " ");      
      for(i=2; i<num_phrases; i++){
	strcat(to_write, parsed[i]);
	if(i != num_phrases-1)
	  strcat(to_write, " ");
      }      
      
      sprintf(*to_fifo, "write|%d|%s", getpid(), to_write);
      return 1;
      
    } else if (strcmp(parsed[0], "@delete") == 0) {
      strcpy(*to_client, "wait");
      sprintf(*to_fifo, "delete|%s", parsed[1]);
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
