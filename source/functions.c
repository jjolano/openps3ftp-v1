/*
    This file is part of OpenPS3FTP.

    OpenPS3FTP is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenPS3FTP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with OpenPS3FTP.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common.h"

void abspath(const char* relpath, const char* cwd, char* abspath)
{
	if(relpath[0] == '/')
	{
		// already absolute, just copy
		strcpy(abspath, relpath);
	}
	else
	{
		// relative, append to cwd and copy
		strcpy(abspath, cwd);
		
		if(cwd[strlen(cwd) - 1] != '/')
		{
			strcat(abspath, "/");
		}
		
		strcat(abspath, relpath);
	}
}

int exists(const char* path)
{
	Lv2FsStat entry;
	return lv2FsStat(path, &entry);
}

int is_dir(const char* path)
{
	Lv2FsStat entry;
	lv2FsStat(path, &entry);
	return fis_dir(entry);
}

int ssplit(const char* str, char* left, int lmaxlen, char* right, int rmaxlen)
{
	int ios = strcspn(str, " ");
	int len = strlen(str);
	int i = 0;
	
	while(i < ios && i < lmaxlen)
	{
		left[i] = str[i];
		i++;
	}
	
	left[i++] = '\0';
	
	if(ios < (len - 1))
	{
		while(i < len && (i - ios) < rmaxlen)
		{
			right[i - ios - 1] = str[i];
			i++;
		}
		
		right[i - ios - 1] = '\0';
		return 1;
	}
	
	right[0] = '\0';
	return 0;
}

int slisten(int port, int backlog)
{
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	
	int list_s = socket(AF_INET, SOCK_STREAM, 0);
	
	bind(list_s, (struct sockaddr *)&sa, sizeof(sa));
	listen(list_s, backlog);
	
	return list_s;
}

int sconnect(const char ipaddr[16], int port, int *sd)
{
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(ipaddr);
	
	return connect((*sd = socket(AF_INET, SOCK_STREAM, 0)), (struct sockaddr *)&sa, sizeof(sa));
}

void sclose(int *sd)
{
	if(*sd > -1)
	{
		shutdown(*sd, SHUT_RDWR);
		closesocket(*sd);
		*sd = -1;
	}
}

int sendfile(const char* filename, int sd, int rest)
{
	int ret = -1;
	char *buf = malloc(BUFFER_SIZE * sizeof(char));
	
	if(buf != NULL)
	{
		Lv2FsFile fd;
		if(lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0) == 0)
		{
			ret = 0;
			
			u64 read, pos;
			lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
			
			while(lv2FsRead(fd, buf, BUFFER_SIZE, &read) == 0 && read > 0)
			{
				if(/*lv2FsFsync(fd) == -1 ||*/ (u64)send(sd, buf, (size_t)read, 0) < read)
				{
					ret = -1;
					break;
				}
			}
			
			lv2FsClose(fd);
		}
		
		free(buf);
	}
	
	return ret;
}

int recvfile(const char* filename, int sd, int rest)
{
	int ret = -1;
	char *buf = malloc(BUFFER_SIZE * sizeof(char));
	
	if(buf != NULL)
	{
		Lv2FsFile fd;
		if(lv2FsOpen(filename, LV2_O_WRONLY | LV2_O_CREAT | (rest == 0 ? LV2_O_TRUNC : 0), &fd, 0644, NULL, 0) == 0)
		{
			ret = 0;
			
			u64 read, write, pos;
			lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
			
			while((read = (u64)recv(sd, buf, BUFFER_SIZE, MSG_WAITALL)) > 0)
			{
				if(/*lv2FsFsync(fd) == -1 ||*/ lv2FsWrite(fd, buf, BUFFER_SIZE, &write) != 0 || write < read)
				{
					ret = -1;
					break;
				}
			}
			
			lv2FsClose(fd);
		}
		
		free(buf);
	}
	
	return ret;
}
