#include <stdio.h>
#include <stdlib.h>
#include "list.h"

/* Prints contents of list pointed to by sp */
char *print_list(struct node *sp){
  char *s = (char *)malloc(128 * sizeof(char));
  /* Make sure pointer to struct node is not null */
  while(sp){
    sprintf(s, "%s%s ", s, (sp->c).user_name);
    sp = sp->next;
  }

  return s;
}

/* Inserts node into front of list */
struct node *insert_front(struct node *sp, struct client ct){
	
  /* Allocates memory in the heap */
  struct node *to_add = (struct node *)malloc(sizeof(struct node) * 1);
  to_add->c = ct;
  to_add->next = sp;

  return to_add;
}

/* delete one member from the list */
struct node *delete_member(struct node *sp, struct client cl){
  struct node *head = sp;
  if (sp != NULL && ((sp->c).client_sub_pid == cl.client_sub_pid)) {
    sp = sp->next;
    free(head);
    return sp;
  }

  while(sp != NULL && sp->next != NULL) {
    if ((sp->next->c).client_sub_pid != cl.client_sub_pid) {
      sp = sp->next;
    }
  }
  
  if (sp->next != NULL) {
    struct node *to_remove = sp->next;
    sp->next = to_remove->next;
    free(to_remove);
  }
    
  return head;
}

/* Frees up all nodes */
struct node *free_list(struct node *sp){
  while(sp != NULL){
    struct node *old = sp;
    sp = sp->next;
    free(old);
    old = NULL;
  }

  return sp;
}
