#define SEM_KEY 12345

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


int create_semaphore(int);
int get_semaphore(int);
void wait_semaphore(int);
void free_semaphore(int);
int remove_semaphore(int);
