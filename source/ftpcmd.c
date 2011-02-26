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
		close(*socket);
		*socket = -1;
	}
}

int recvfile(int socket, const char filename[256], int bufsize, s64 startpos)
{
	int ret = 0;
	Lv2FsFile fd;
	
	char *buf = malloc(bufsize);
	
	// if no buff is available, then no need to open file :0)
	if(buf != NULL && lv2FsOpen(filename, LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0) == 0)
	{
		u64 pos, bytes, written = 0;
		lv2FsLSeek64(fd, startpos, SEEK_SET, &pos);
		
		while((bytes = (u64)recv(socket, buf, bufsize, MSG_WAITALL)) > 0)
		{
			if(lv2FsWrite(fd, buf, bytes, &written) != 0 || written < bytes)
			{
				// error
				ret = -1;
				break;
			}
		}
		
		lv2FsClose(fd);
		free(buf);
	}
	
	return ret;
}

int sendfile(int socket, const char filename[256], int bufsize, s64 startpos)
{
	int ret = 0;
	Lv2FsFile fd;
	
	char *buf = malloc(bufsize);
	
	// if no buff is available, then no need to open file :0)
	if(buf != NULL && lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0) == 0)
	{
		u64 pos, read;
		lv2FsLSeek64(fd, startpos, SEEK_SET, &pos);
		
		while(lv2FsRead(fd, buf, bufsize, &read) == 0 && read > 0)
		{
			if(send(socket, buf, (size_t)read, MSG_WAITALL) < read)
			{
				// error
				ret = -1;
				break;
			}
		}
		
		lv2FsClose(fd);
		free(buf);
	}
	
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
			listcb(&entry);
			count++;
		}
	}
	else
	{
		count = -1;
	}
	
	lv2FsCloseDir(fd);
	return count;
}

