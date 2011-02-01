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

#include "functions.h"

#include <string.h>
#include <fcntl.h>

void absPath(char* absPath, const char* path, const char* cwd)
{
	if(strlen(path) > 0 && path[0] == '/')
	{
		strcpy(absPath, path);
	}
	else
	{
		strcpy(absPath, cwd);
		strcat(absPath, path);
	}
}

int exists(char* path)
{
	struct stat entry; 
	return stat(path, &entry);
}

int isDir(char* path)
{
	struct stat entry; 
	stat(path, &entry);
	return ((entry.st_mode & S_IFDIR) != 0);
}

void stoupper(char *s)
{
	do if (96 == (224 & *s)) *s &= 223;
	while (*s++);
}
