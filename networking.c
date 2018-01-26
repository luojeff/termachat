#define _GNU_SOURCE
#include "networking.h"
#include "helper.h"

void error_check( int i, char *s ) {
  if ( i < 0 ) {
    printf("[%s] error %d: %s\n", s, errno, strerror(errno) );
    exit(1);
  }
}

/*=========================
  server_setup
  args:

  creates, binds a server side socket
  and sets it to the listening state

  returns the socket descriptor
  =========================*/
int server_setup(int port) {  
  int sd, i;

  //create the socket
  sd = socket( AF_INET, SOCK_STREAM, 0 );
  error_check( sd, "server socket" );
  printf("[NETWORK]: socket created\n");

  //setup structs for getaddrinfo
  struct addrinfo *hints, *results;
  hints = calloc(1, sizeof(struct addrinfo));
  hints->ai_family = AF_INET;  // IPv4 address
  hints->ai_socktype = SOCK_STREAM;  // TCP socket
  hints->ai_flags = AI_PASSIVE;  // Use all valid addresses

  getaddrinfo(NULL, int_to_str(port), hints, &results);
  
  /* Attempt successive ports */
  int inc = 0;
  while((i = bind( sd, results->ai_addr, results->ai_addrlen)) != 0) {
    if(inc < ATTEMPTS){
      inc++;
      getaddrinfo(NULL, int_to_str(port + inc), hints, &results);
    } else
      break;
  }

  port += inc;
  
  error_check( i, "server bind" );
  printf("[NETWORK]: socket bound to port %s\n", int_to_str(port));

  
  //set socket to listen state
  i = listen(sd, 10);
  error_check( i, "server listen" );
  printf("[NETWORK]: socket at port %s in listen state\n", int_to_str(port));

  //free the structs used by getaddrinfo
  free(hints);
  freeaddrinfo(results);
  return sd;
}


/*=========================
  server_connect
  args: int sd

  sd should refer to a socket in the listening state
  run the accept call

  returns the socket descriptor for the new socket connected
  to the client.
  =========================*/
int server_connect(int sd) {
  //printf("server_connect %d starting\n", sd);
  int to_client;
  socklen_t sock_size = 0;
  struct sockaddr_un client_socket;

  int i;

  //printf("about to accept connection\n");
  to_client = accept4(sd, (struct sockaddr *)&client_socket, &sock_size, SOCK_NONBLOCK);
  //printf("connection accepted\n");
  if(to_client == -1)
    printf("ERROR: %s\n", strerror(errno));

  printf("[NETWORK]: new connection with %d\n", to_client);
  
  return to_client;
}

/*=========================
  client_setup
  args: int * server

  server is a string representing the server address

  port is self-explanatory

  create and connect a socket to a server socket that is
  in the listening state

  returns the file descriptor for the socket
  =========================*/
int client_setup(char *server, char *port) {
  int sd, i, inc = 0;

  //create the socket
  sd = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 );
  error_check( sd, "client socket" );

  //run getaddrinfo
  /* hints->ai_flags not needed because the client
     specifies the desired address. */
  struct addrinfo * hints, * results;
  hints = calloc(1, sizeof(struct addrinfo));
  hints->ai_family = AF_INET;  //IPv4
  hints->ai_socktype = SOCK_STREAM;  //TCP socket

  getaddrinfo(server, port, hints, &results);

  //connect to the server
  //connect will bind the socket for us
  i = connect( sd, results->ai_addr, results->ai_addrlen );
  
  error_check( 1, "client connect");

  free(hints);
  freeaddrinfo(results);

  return sd;
}
