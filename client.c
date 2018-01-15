#include "networking.h"
#include "helper.h"

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
  
  char *ip_addr, buffer[BUFFER_SIZE];

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

  while (1) {
    printf("[YOU @ %s]: ", current_group);
    fgets(buffer, sizeof(buffer), stdin);
    *strchr(buffer, '\n') = 0;

    if(strcmp(buffer, "join chatroom1") == 0) {
      write(current_socket, buffer, sizeof(buffer));
      read(current_socket, buffer, sizeof(buffer));
      printf("Joining chatroom %s:%s\n", ip_addr, buffer);

      /* Gets next additional port */
      int server_sock_1;
      if((server_sock_1 = client_setup( ip_addr, buffer)) != -1)
	printf("Connected to chatroom %s:%s!\n", ip_addr, buffer);
      else
	printf("Error: failed to connect!\n");
      
      //printf("SERVER_SOCK_1: %d\n", server_sock_1);

      strcpy(current_group, "CR-");
      strncat(current_group, buffer, MAX_GROUP_NAME_SIZE);
      current_socket = server_sock_1;
    } else {
      write(current_socket, buffer, sizeof(buffer));
      read(current_socket, buffer, sizeof(buffer));
      printf("[%s]: [%s]\n", current_group, buffer);
    }
  }

  
}
