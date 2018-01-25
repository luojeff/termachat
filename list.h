#ifndef LIST_H
#define LIST_H

#include "forking_server.h"

struct node {
  struct client c;
  struct node *next;
};

char *print_list(struct node *);
struct node *insert_front(struct node *, struct client);
struct node *delete_member(struct node *, struct client);
struct node *free_list(struct node *);

#endif
