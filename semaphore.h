union semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
  struct seminfo *__buf;
};

int create_semaphore(int);
int get_semaphore(int);
void wait_semaphore(int);
void free_semaphore(int);
void remove_semaphore(int*);
int get_sem_val(int);
int is_used(int);
