#include "networking.h"
#include "helper.h"

char *int_to_str(int val){
  int SIZE = 10; //(int)((ceil(log10(val))+1)*sizeof(char));
  
  char *str = malloc(SIZE * sizeof(char));

  sprintf(str, "%d", val);
  return str;
}

void print_error(){
  printf("Error from P%d: %s\n", getpid(), strerror(errno));
  exit(0);
}
