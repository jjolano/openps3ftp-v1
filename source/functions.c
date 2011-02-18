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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <net/net.h>

#include <psl1ght/lv2/filesystem.h>

#include "md5.h"
#include "functions.h"

void absPath(char* absPath, const char* path, const char* cwd)
{
	if(path[0] == '/')
	{
		strcpy(absPath, path);
	}
	else
	{
		strcpy(absPath, cwd);
		strcat(absPath, path);
	}
}

int exists(const char* path)
{
	Lv2FsStat entry;
	return lv2FsStat(path, &entry);
}

int isDir(const char* path)
{
	Lv2FsStat entry;
	lv2FsStat(path, &entry);
	return ((entry.st_mode & S_IFDIR) != 0);
}

/*void stoupper(char *s)
{
	do if (96 == (224 & *s)) *s &= 223;
	while (*s++);
}*/

void md5(char md5[33], const char* str)
{
	unsigned char md5sum[16];

	md5_context ctx;
	md5_starts(&ctx);
	md5_update(&ctx, (unsigned char *)str, strlen(str));
	md5_finish(&ctx, md5sum);

	int i;
	for(i = 0; i < 16; i++)
	{
		sprintf(md5 + i * 2, "%02x", md5sum[i]);
	}
}

