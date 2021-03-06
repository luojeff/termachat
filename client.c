#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"
#include "parser.h"

void handle_sub_response(char *, int, char (*)[]);
int handle_user_input(int, int, char *);
int receive_message(int, char *);


static void client_sighandler(int signo){
  switch(signo) {
    //case SIGINT:
    printf("Client process quit due to SIGINT\n");
    //exit(0);
    break;
  case SIGUSR1:
    printf("Test signal!\n");
    break;
  }
}


int main(int argc, char **argv) {

  /* Handle user signals */
  signal(SIGINT, client_sighandler);
  signal(SIGUSR1, client_sighandler);

  int server_socket, current_socket;

  /* Current location of USER */
  char current_group[MAX_GROUPNAME_SIZE];
  strcpy(current_group, "PUB");

  printf("------------------- Termachat --------------------\n");
  
  char *ip_addr;
  if (argc == 2) {
    ip_addr = argv[1];
    printf("Attempting to connect to %s:%s\n", ip_addr, int_to_str(PORT));
    if((server_socket = client_setup(ip_addr, int_to_str(PORT))) != -1)    
      printf("Connected to server!\n");
    else
      printf("Error: failed to connect!\n");
  } else {
    ip_addr = TEST_IP;
    printf("Attempting to connect to %s:%s\n", ip_addr, int_to_str(PORT));
    if((server_socket = client_setup(ip_addr, int_to_str(PORT))) != -1)
      printf("Connected to server!\n");
    else
      printf("Error: failed to connect!\n");
  }

  current_socket = server_socket;

  char user_name[MAX_USERNAME_LENGTH];
  printf("Enter a username: ");
  fgets(user_name, sizeof(user_name), stdin);
  *strchr(user_name, '\n') = 0;

  write(current_socket, user_name, sizeof(user_name));

  char input_buffer[BUFFER_SIZE];
  while (1) {
    
    printf("[%s @ %s]: ", user_name, current_group);
    fgets(input_buffer, sizeof(input_buffer), stdin);
    *strchr(input_buffer, '\n') = 0;

    //char **client_parsed = parse_input(input_buffer, " ");
    //printf("First arg passed: [%s]\n", client_parsed[0]);

    write(current_socket, input_buffer, sizeof(input_buffer));
    read(current_socket, input_buffer, sizeof(input_buffer));

    //printf("DEBUG:: Just received: %s:\n", input_buffer);
    if (strcmp(input_buffer, "wait") == 0) {
      read(current_socket, input_buffer, sizeof(input_buffer));
    }
    
    handle_sub_response(input_buffer, current_socket, &current_group);
  }
}

int handle_user_input(int input, int socket, char *input_buffer){
  int bytes_read;
  while((bytes_read = read(input, &input_buffer, BUFFER_SIZE)) < 0);
  if(bytes_read > 0){
    write(socket, &input_buffer, BUFFER_SIZE);
  } 
  return bytes_read;
}

int receive_message(int socket, char *outside_buffer){
  int bytes_read;
  bytes_read = read(socket, &outside_buffer, BUFFER_SIZE);
  if(bytes_read > 0){
    char *new_line;
    if((new_line = strchr(outside_buffer, '\n')) != NULL) *new_line = '\0';
    if (strcmp(outside_buffer, "wait") == 0) {
      read(socket, outside_buffer, sizeof(outside_buffer));
    }
    printf("Received: [%s]\n", outside_buffer); 
  }
  
  return bytes_read;
}

void handle_sub_response(char *input_buffer, int current_socket, char (*current_group)[]){
  char **client_parsed, **args;
  
  if(count_occur(input_buffer, "#") != 2){
    printf("Received invalid response by server! Check server code!\n");
    exit(0);
  }

  client_parsed = parse_input(input_buffer, "#");
  args = parse_input(client_parsed[1], "|");

  char type = args[0][0];
  if(type == 'c'){
    char *command = args[1];

    // Typical commands
    if(strcmp("display-invalid", command) == 0){
      printf("Invalid command! Type @help for help.\n");
      
    } else if (strcmp("display-help", command) == 0){
      int fd = open("help", O_RDONLY);
      char contents[1024];
      memset(contents, 0, sizeof(contents));
      read(fd, contents, sizeof(contents));
      printf("%s\n", contents);
      close(fd);
    
    } else if (strcmp("join", command) == 0) {
      printf("Joined the room: %s\n", args[2]);

      strcpy(*current_group, args[2]);
  
    } else if(strcmp("exit", command) == 0){
      printf("Ended subprocess. Exiting...\n");
      exit(0);
      
    }  else {
      printf("No handling case for client -- check server code!\n");
    }
    
  } else if (type == 't'){
    char *sender = args[1];
    char *text = args[2];

    /* Text */
    if(strcmp(sender, "SERVER") == 0)
      printf("[%s]: %s\n", sender, text);
    else
      printf("[%s]: [%s]\n", sender, text);    
  } else if (type == 'o'){
    char *next = args[1];


    // *Other* responses from server
    if(strcmp("not-in-chatroom", next) == 0){
      printf("You must be in a chatroom to perform that operation!\n");
    } else if(strcmp("chatroom-noexist", next) == 0){      
      printf("Chatroom does not exist!\n");      
    } else if(strcmp("chatroom-nametaken", next) == 0){
      printf("Chatroom w/ given name already exists!\n");
      
      /*    } else if(strcmp("requesting", next) == 0){
      
	    printf("Requesting from server...\n");

	    char sub_response[BUFFER_SIZE];
	    read(current_socket, sub_response, sizeof(sub_response));

	    // Recursive implementation
	    handle_sub_response(sub_response, current_socket);
      */
    } else if(strcmp("chatroom-created", next) == 0) {
      printf("Chatroom created!\n");
    } else if(strcmp("client-noexist", next) == 0) {
      printf("Client process doesn't exist!\n");
    } else if(strcmp("already-joined-other-chatroom", next) == 0) {
      printf("Already in another chatroom. @leave before joining a new one!\n");
    } else if(strcmp("read", next) == 0) {
      printf("--- Previous Messages ---\n");
      printf("%s\n", args[2]);
    } else if(strcmp("hread", next) == 0) {
      printf("--- Message History ---\n");
      printf("%s\n", args[2]);
    } else if(strcmp("written", next) == 0) {
      //printf("Wrote to chat!\n"); // FOR TESTING
    } else if(strcmp("left-chat", next) == 0) {
      strcpy(*current_group, "PUB");
      printf("Left chatroom %s!\n", args[2]);
    } else if(strcmp("unable-leave", next) == 0) {      
      printf("Unable to leave %s. @exit to quit client and subprocess\n", *current_group);
    } else if (strcmp("chat-has-members", next) == 0) {
      printf("Unable to delete -- chatroom has members!\n");
    } else if (strcmp("delete-success", next) == 0) {
      printf("Chatroom successfully deleted!\n");
    } else {
      printf("Command unavailable!\n");
    }
  } else {
    /* Invalid response from server! */
  }

  free(client_parsed);
  free(args); 
}
