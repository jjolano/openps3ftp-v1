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

#include <net/net.h>
#include <psl1ght/lv2/filesystem.h>

#include "common.h"

int slisten(int port, int backlog)
{
	int list_s = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in sa;
	socklen_t sin_len = sizeof(sa);
	memset(&sa, 0, sin_len);
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	
	bind(list_s, (struct sockaddr *)&sa, sin_len);
	listen(list_s, backlog);
	
	return list_s;
}

int sconnect(int *ret_s, const char ipaddr[16], int port)
{
	*ret_s = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in sa;
	socklen_t sin_len = sizeof(sa);	
	memset(&sa, 0, sin_len);
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(ipaddr);
	
	return connect(*ret_s, (struct sockaddr *)&sa, sin_len);
}

void sclose(int *socket)
{
	if(*socket != -1)
	{
		shutdown(*socket, SHUT_RDWR);
		closesocket(*socket);
		*socket = -1;
	}
}

int recvfile(int socket, const char filename[256], int rest)
{
	int ret = -1;
	Lv2FsFile fd;
	
	char *buf = malloc(BUFFER_SIZE);
	
	if(buf != NULL && lv2FsOpen(filename, LV2_O_WRONLY | LV2_O_CREAT, &fd, 0644, NULL, 0) == 0)
	{
		u64 read, write = 0, pos;
		
		if(rest > 0)
		{
			lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
		}
		else
		{
			lv2FsFtruncate(fd, 0);
		}
		
		ret = 0;
		
		while((read = (u64)recv(socket, buf, BUFFER_SIZE, MSG_WAITALL)) > 0)
		{
			if(lv2FsWrite(fd, buf, read, &write) != 0 || write < read)
			{
				// write error
				ret = -1;
				break;
			}
		}
		
		lv2FsClose(fd);
		free(buf);
	}
	
	return ret;
}

int sendfile(int socket, const char filename[256], int rest)
{
	int ret = -1;
	Lv2FsFile fd;
	
	char *buf = malloc(BUFFER_SIZE);
	
	if(buf != NULL && lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0) == 0)
	{
		u64 read, pos;
		
		if(rest > 0)
		{
			lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
		}
		
		ret = 0;
		
		while(lv2FsRead(fd, buf, BUFFER_SIZE, &read) == 0 && read > 0)
		{
			if((u64)send(socket, buf, (size_t)read, 0) < read)
			{
				// send error
				ret = -1;
				break;
			}
		}
		
		lv2FsClose(fd);
		free(buf);
	}
	
	return ret;
}

