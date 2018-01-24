#include "networking.h"
#include "semaphore.h"
#include "helper.h"


// Creates and decrements a semaphore
int create_semaphore(int sem_key){
  struct sembuf sbuf = {0, -1, SEM_UNDO};
  union semun su;  
  su.val = 1;
  
  int sem_id;  
  if((sem_id = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0644)) < 0)
    print_error();
  else
    semctl(sem_id, 0, SETVAL, su);  

  return sem_id;
}


int get_semaphore(int sem_key){
  int sem_id;
  if((sem_id = semget(sem_key, 0, 0644)) < 0)
    print_error();
  
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
void remove_semaphore(int *sem_id){
  struct sembuf sbuf = {0, 1, SEM_UNDO};
  
  // Wait until semaphore freed and delete semaphore
  // Remove semaphore
  if((semop(*sem_id, &sbuf, 1)) < 0)
    print_error();
  if(semctl(*sem_id, 1, IPC_RMID) < 0)
    print_error();

  *sem_id = 0;
}

int get_sem_val(int sem_id){
  union semun sem;
  int val = semctl(sem_id, 0, GETVAL, sem);
  return val;
}

// Returns 1 if semaphore is used
int is_used(int sem_id){
  int val = get_sem_val(sem_id);
  if(val == 0)
    return 1;
  else if(val > 0)
    return 0;
  else
    return -1;
}
