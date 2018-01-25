struct node {
  int i;
  struct node *next;
};

char *print_list(struct node *);
struct node *insert_front(struct node *, int);
struct node *delete_member(struct node *, int);
struct node *free_list(struct node *);
