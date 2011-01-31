/*

  HELPER.C
  ========
  (c) Paul Griffiths, 1999
  Email: mail@paulgriffiths.net

  Implementation of sockets helper functions.

  Many of these functions are adapted from, inspired by, or 
  otherwise shamelessly plagiarised from "Unix Network 
  Programming", W Richard Stevens (Prentice Hall).

  Modified by jjolano for OpenPS3FTP.

*/

#include "helper.h"
#include <sys/socket.h>
#include <string.h>
#include <net/net.h>
#include <unistd.h>
#include <errno.h>


/*  Read a line from a socket  */

ssize_t sreadl(int socket, char *buffer, int maxlen){
	ssize_t n;
	ssize_t read;
	char c;

	for(n = 1; n < maxlen; n++)
	{
		if((read = netRecv(socket, &c, 1, 0)) == 1)
		{
			*buffer++ = c;
			
			if(c == '\n')
			{
				break;
			}
		}
		else if(read == 0)
		{
			if(n == 1)
			{
				return 0;
			}
			else
			{
				break;
			}
		}
		else
		{
			if(errno == EINTR)
			{
				continue;
			}
			
			return -1;
		}
    }

    *buffer = 0;
    return n;
}


/*  Write a line to a socket  */

ssize_t swritel(int socket, const char *str)
{
	int n = strlen(str);
	
	if(n == 0)
	{
		return 0;
	}
	
	size_t      nleft;
	ssize_t     write;
	const char *buffer;
	
	buffer = str;
	nleft  = n;

	while(nleft > 0)
	{
		if((write = netSend(socket, buffer, nleft, 0)) <= 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			
			return -1;
		}
		
		nleft  -= write;
		buffer += write;
	}
	
	return n;
}

