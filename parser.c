#include "networking.h"
#include "parser.h"


/*
  Parses string line by delimiter, returning an array to pointers.
*/
char **parse_input(char *line, char *delimiter){
  int i=0;
  char *curr_arg = NULL;
  char **ret_args = (char **)malloc(sizeof(char *) * (count_occur(line, delimiter)+2));
  
  while(curr_arg = strsep(&line, delimiter)){
    ret_args[i] = curr_arg;
    i++;
  }

  ret_args[i] = NULL;

  return ret_args;
}


/* 
   Returns substring of size characters, not including terminating null
*/
char *trim_string(char *s, int size) {
  int i;
  char *ret_str = (char *)malloc(size+1);
  
  ret_str[size] = '\0';
  
  for(i=0; i<size; i++)
    ret_str[i] = s[i];

  return ret_str;
}


/* 
   Trims characters c from the beginning and end of string s

   Ex.)
   trim_trailing("aaacataa", 'a') -> "cat"
*/
char *trim_trailing(char *s, char c){
  int i, count=0, count2=0;  

  for(i=0; i<strlen(s); i++){
    if(s[i] == c)
      count++;
    else
      break;
  }

  for(i=strlen(s)-1; i>=0; i--){
    if(s[i] == c)
      count2++;
    else
      break;
  }

  int ret_str_size = strlen(s)-count-count2+1;
  char *ret_str = malloc(ret_str_size);

  ret_str[ret_str_size-1] = '\0';

  for(i=count; i<strlen(s)-count2; i++)
    ret_str[i-count] = s[i];

  return ret_str;
}


/* 
   Returns the number of occurrences of substring occ in string s
*/
int count_occur(char *s, char *occ){
  int count = 0;
  char *rem = s;

  while(rem = strstr(rem, occ)){
    count++;
    rem++;
  }

  return count;
}

/*
  Returns the number of phrases in a string s. Delimited by char c.
*/
int get_num_phrases(char *s, char c){
  int i, np = 0, vp = 0;
  for(i=0; i<strlen(s); i++){
    if(s[i] == c && vp) {
      np++;
      vp = 0;
    } else if (s[i] != c){
      vp = 1;
    }      
  }  

  if(strlen(s) > 0 && s[strlen(s)-1] != ' ')
    np++; // account for last phrase

  return np;
}
