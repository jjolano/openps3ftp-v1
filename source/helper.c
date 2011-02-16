//    This file is part of OpenPS3FTP.

//    OpenPS3FTP is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    OpenPS3FTP is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with OpenPS3FTP.  If not, see <http://www.gnu.org/licenses/>.

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
	
	nleft  = n;

	while(nleft > 0)
	{
		if((write = netSend(socket, str, nleft, 0)) <= 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			
			return -1;
		}
		
		nleft	-= write;
		str	+= write;
	}
	
	return n;
}

