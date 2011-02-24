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

#include <psl1ght/lv2/filesystem.h>

#include <net/net.h>

#include <malloc.h>

#include "common.h"

int ssend(int socket, const char* str)
{
	return send(socket, str, strlen(str), 0);
}

int slisten(int port)
{
	int list_s = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	
	sa.sin_family		= AF_INET;
	sa.sin_port		= htons(port);
	sa.sin_addr.s_addr	= htonl(INADDR_ANY);
	
	bind(list_s, (struct sockaddr *)&sa, sizeof(sa));
	listen(list_s, 8);
	
	return list_s;
}

int sconnect(int *ret, const char ipaddr[16], int port)
{
	int conn_s = socket(AF_INET, SOCK_STREAM, 0);
	
	*ret = conn_s;
	
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	
	sa.sin_family		= AF_INET;
	sa.sin_port		= htons(port);
	sa.sin_addr.s_addr	= inet_addr(ipaddr);
	
	return connect(conn_s, (struct sockaddr *)&sa, sizeof(sa));
}

void sclose(int *socket)
{
	if(*socket != -1)
	{
		shutdown(*socket, SHUT_RDWR);
		close(*socket);
		*socket = -1;
	}
}

int recvfile(int socket, const char filename[256], int bufsize, s64 startpos)
{
	int ret = -1;
	Lv2FsFile fd;
	
	if(lv2FsOpen(filename, LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0) == 0)
	{
		char *buf = malloc(bufsize);
		
		if(buf != NULL)
		{
			u64 pos, written = 0;
			
			lv2FsLSeek64(fd, startpos, SEEK_SET, &pos);
			
			while(recv(socket, buf, bufsize, 0) > 0)
			{
				lv2FsWrite(fd, buf, (u64)bufsize, &written);
				
				if(written < (u64)bufsize)
				{
					break;
				}
			}
			
			ret = 0;
			free(buf);
		}
	}
	
	lv2FsClose(fd);
	return ret;
}

int sendfile(int socket, const char filename[256], int bufsize, s64 startpos)
{
	int ret = -1;
	Lv2FsFile fd;
	
	if(lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0) == 0)
	{
		char *buf = malloc(bufsize);
		
		if(buf != NULL)
		{
			u64 pos, read;
			
			lv2FsLSeek64(fd, startpos, SEEK_SET, &pos);
			
			while(lv2FsRead(fd, buf, bufsize, &read) == 0 && read > 0)
			{
				send(socket, buf, (size_t)read, 0);
				
				if(read < (u64)bufsize)
				{
					break;
				}
			}
			
			ret = 0;
			free(buf);
		}
	}
	
	lv2FsClose(fd);
	return ret;
}

int slist(const char dir[256], void (*listcb)(Lv2FsDirent *entry))
{
	int count = 0;
	Lv2FsFile fd;
	
	if(lv2FsOpenDir(dir, &fd) == 0)
	{
		Lv2FsDirent entry;
		u64 read;
		
		while(lv2FsReadDir(fd, &entry, &read) == 0 && read > 0)
		{
			count++;
			listcb(&entry);
		}
	}
	else
	{
		count = -1;
	}
	
	lv2FsCloseDir(fd);
	return count;
}

