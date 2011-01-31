/*

  HELPER.H
  ========
  (c) Paul Griffiths, 1999
  Email: mail@paulgriffiths.net

  Interface to socket helper functions. 

  Many of these functions are adapted from, inspired by, or 
  otherwise shamelessly plagiarised from "Unix Network 
  Programming", W Richard Stevens (Prentice Hall).

  Modified by jjolano for OpenPS3FTP.

*/


#ifndef PG_SOCK_HELP
#define PG_SOCK_HELP

#include <unistd.h>

#define LISTENQ        (1024)   /*  Backlog for listen()   */


/*  Function declarations  */

ssize_t sreadl(int socket, char *buffer, int maxlen);
ssize_t swritel(int socket, const char *str);

#endif  /*  PG_SOCK_HELP  */
