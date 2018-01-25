#include "networking.h"
#include "helper.h"
#include "parser.h"

void handle_sub_response(char *, int);

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

  int server_socket;

  /* Current socket USER is on */
  int current_socket;

  /* Current location of USER */
  int MAX_GROUP_NAME_SIZE = 32;
  char current_group[MAX_GROUP_NAME_SIZE];
  strcpy(current_group, "PUB");
  
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

  char input_buffer[BUFFER_SIZE];

  while (1) {
    printf("[YOU @ %s]: ", current_group);
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
    
    handle_sub_response(input_buffer, current_socket);

    /*
      if(strcmp(client_parsed[0], "@join") == 0) {
      strcat(contents, parsed[1]);
      write(current_socket, contents, sizeof(contents));
      read(current_socket, input_buffer, sizeof(input_buffer));
      printf("Joining chatroom %s:%s\n", ip_addr, input_buffer);

      /* Gets next additional port */ /*
	 int server_sock;
	 if((server_sock = client_setup( ip_addr, input_buffer)) != -1)
	 printf("Connected to chatroom %s:%s!\n", ip_addr, input_buffer);
	 else
	 printf("Error: failed to connect!\n");
      
	 //printf("SERVER_SOCK: %d\n", server_sock);

	 strcpy(current_group, "CR-");
	 strncat(current_group, input_buffer, MAX_GROUP_NAME_SIZE);
	 current_socket = server_sock;
	 } else {
	 write(current_socket, input_buffer, sizeof(input_buffer));
	 read(current_socket, input_buffer, sizeof(input_buffer));
      
	 if(strcmp(input_buffer, "chatroom-success") == 0)
	 printf("Chatroom created!\n");
	 else
	 printf("[%s]: [%s]\n", current_group, input_buffer);
	 } */
  }

     
}

void handle_sub_response(char *input_buffer, int current_socket){
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
      char contents[512];
      read(fd, contents, sizeof(contents));
      printf("%s\n", contents);
      close(fd);
    
    } else if (strcmp("join", command) == 0) {
      printf("Joined the room: %s\n", args[2]);
  
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
    if(strcmp("chatroom-success", next) == 0){
      
      printf("Chatroom successfully created!\n");
      
    } else if(strcmp("chatroom-noexist", next) == 0){
      
      printf("Chatroom does not exist!\n");
      
    } else if(strcmp("chatroom-nametaken", next) == 0){
      
      printf("Chatroom w/ input name already exists!\n");
      
      /*    } else if(strcmp("requesting", next) == 0){
      
      printf("Requesting from server...\n");

      char sub_response[BUFFER_SIZE];
      read(current_socket, sub_response, sizeof(sub_response));

      // Recursive implementation
      handle_sub_response(sub_response, current_socket);
      */
    } else if(strcmp("chatroom-created", next) == 0) {

      printf("Chatroom Created\n");

    } else if(strcmp("client-noexist", next) == 0) {

      printf("Client Process does not exist!\n");

    } else if(strcmp("already-joined-other-chatroom", next) == 0) {

      printf("Already joined other room!\n");
    } else if(strcmp("response", next) == 0) {

      printf("{Replace w/ chatroom chat stuff...}\n");
    } else if(strcmp("written", next) == 0){

      printf("Wrote to chat!\n");
    } else {
      printf("No handling case for client -- check server code!\n");
    }
  } else {
    /* Invalid response from server! */
  }

  free(client_parsed);
  free(args); 
}
