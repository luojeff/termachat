#include "networking.h"

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
  int server_socket2;
  char *current = "PUBLIC";
  char buffer[BUFFER_SIZE];

  if (argc == 2) {
    server_socket = client_setup( argv[1]);
    //server_socket2 = client_setup( argv[1], "10002" );
  } else {
    server_socket = client_setup( TEST_IP);
    //server_socket2 = client_setup( TEST_IP, "10002" );
  }

  while (1) {
    printf("[USER]: ");
    fgets(buffer, sizeof(buffer), stdin);
    *strchr(buffer, '\n') = 0;
    write(server_socket, buffer, sizeof(buffer));
    read(server_socket, buffer, sizeof(buffer));
    printf("received from 1: [%s]\n", buffer);
    //write(server_socket2, buffer, sizeof(buffer));
    //read(server_socket2, buffer, sizeof(buffer));
    //printf("[%s]: [%s]\n", current, buffer);
  }
}
