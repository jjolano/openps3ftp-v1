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
#include <lv2/process.h>

#include <fcntl.h>

#include "common.h"
#include "helper.h"
#include "ftpcmd.h"

void cmd_user(const char* param, int conn_s, char username[32])
{
	if(strlen(param) > 0)
	{
		char output[64];
		strncpy(username, param, 31);
		sprintf(output, "331 Username %s OK. Password required\r\n", param);
		swritel(conn_s, output);
	}
	else
	{
		swritel(conn_s, "501 A username is required for login\r\n");
	}
}

int cmd_pass(const char* param, int conn_s, const char* cmp)
{
	if(strlen(param) > 0)
	{
		char passwd[33];
		md5(passwd, param);
		
		if(strlen(cmp) == 0 || strncmp(passwd, cmp, 32) == 0)
		{
			swritel(conn_s, "230 Successful authentication\r\n");
			return 0;
		}
		else
		{
			swritel(conn_s, "430 Invalid username or password\r\n");
		}
	}
	else
	{
		swritel(conn_s, "501 Invalid username or password\r\n");
	}
	
	return -1;
}

void cmd_pasv(int conn_s, int *data_s, u32 *rest)
{
	*rest = 0;
	
	closeconn(*data_s);
	
	netSocketInfo snf;
	
	if(netGetSockInfo(conn_s, &snf, 1) == 0)
	{
		srand(conn_s);
		
		int p1 = (rand() % 251) + 4;
		int p2 = rand() % 256;
		
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family      = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port        = htons((p1 * 256) + p2);
		
		*data_s = netSocket(AF_INET, SOCK_STREAM, 0);
		netBind(*data_s, (struct sockaddr *) &servaddr, sizeof(servaddr));
		netListen(*data_s, 1);
		
		char output[64];
		sprintf(output, "227 Entering Passive Mode (%u,%u,%u,%u,%i,%i)\r\n",
			(snf.local_adr.s_addr & 0xFF000000) >> 24,
			(snf.local_adr.s_addr & 0xFF0000) >> 16,
			(snf.local_adr.s_addr & 0xFF00) >> 8,
			(snf.local_adr.s_addr & 0xFF),
			p1, p2);
		
		swritel(conn_s, output);
		
		int temp = netAccept(*data_s, NULL, NULL);
		
		if(temp > 0)
		{
			closeconn(*data_s);
			*data_s = temp;
			return;
		}
	}
	
	swritel(conn_s, "550 Data socket error\r\n");
	closeconn(*data_s);
}

void cmd_port(const char* param, int conn_s, int *data_s, u32 *rest)
{
	*rest = 0;
	
	closeconn(*data_s);
	
	char userdata[24];
	strcpy(userdata, param);
	
	char data[6][4];
	char *splitstr = strtok(userdata, ",");
	
	int i = 0;
	while(i < 6 && splitstr != NULL)
	{
		strcpy(data[i++], splitstr);
		splitstr = strtok(NULL, ",");
	}
	
	if(i < 6)
	{
		swritel(conn_s, "501 Syntax error\r\n");
		return;
	}
	
	char ipaddr[16];
	sprintf(ipaddr, "%s.%s.%s.%s", data[0], data[1], data[2], data[3]);
	
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family	= AF_INET;
	servaddr.sin_port	= htons((atoi(data[4]) * 256) + atoi(data[5]));
	inet_pton(AF_INET, ipaddr, &servaddr.sin_addr);
	
	*data_s = netSocket(AF_INET, SOCK_STREAM, 0);
	
	if(connect(*data_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) == 0)
	{
		swritel(conn_s, "200 PORT command successful\r\n");
	}
	else
	{
		swritel(conn_s, "550 PORT command failed\r\n");
		closeconn(*data_s);
	}
}

void cmd_site(const char* param, int conn_s)
{
	char cmd[16];
	char cmdparam[256];
	
	simplesplit(param, cmd, cmdparam);
	
	if(strcasecmp(cmd, "CHMOD") == 0)
	{
		char perms[4], temp[4];
		char filename[256];
		
		simplesplit(cmdparam, temp, filename);
		
		sprintf(perms, "0%i", atoi(temp));
		
		if(lv2FsChmod(filename, S_IFMT | strtol(perms, NULL, 8)) == 0)
		{
			swritel(conn_s, "250 File permissions successfully set\r\n");
		}
		else
		{
			swritel(conn_s, "250 Failed to set file permissions\r\n");
		}
	}
	else
	if(strcasecmp(cmd, "PASSWD") == 0)
	{
		char md5pass[33];
		md5(md5pass, cmdparam);
		
		Lv2FsFile fd;
		u64 written;
		
		if(lv2FsOpen("/dev_hdd0/game/OFTP00001/USRDIR/passwd", LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0) == 0)
		{
			lv2FsWrite(fd, md5pass, 32, &written);
			swritel(conn_s, "200 FTP password successfully changed\r\n");
		}
		else
		{
			swritel(conn_s, "550 Cannot change FTP password\r\n");
		}
		
		lv2FsClose(fd);
	}
	else
	if(strcasecmp(cmd, "EXITAPP") == 0)
	{
		swritel(conn_s, "221 Exiting OpenPS3FTP\r\n");
		closeconn(conn_s);
		sysProcessExit(0);
	}
}

void cmd_feat(int conn_s)
{
	static char *feat_cmds[] =
	{
		"PASV",
		"SIZE",
		"REST STREAM",
		"SITE CHMOD",
		"SITE PASSWD",
		"SITE EXITAPP",
		"MLSD",
		"MLST type*;size*;modify*;UNIX.mode*;UNIX.uid*;UNIX.gid*;",
		"CDUP",
		"PWD",
		"CWD",
		"PORT"
	};
	
	const int feat_cmds_count = sizeof(feat_cmds) / sizeof(char *);
	
	swritel(conn_s, "211-Extensions:\r\n");
	
	char output[64];
	int i;
	
	for(i = 0; i < feat_cmds_count; i++)
	{
		sprintf(output, " %s\r\n", feat_cmds[i]);
		swritel(conn_s, output);
	}
	
	swritel(conn_s, "211 End\r\n");
}

void cmd_retr(const char* param, int conn_s, int data_s, u32 rest)
{
	if(data_s == -1)
	{
		swritel(conn_s, "425 No data connection\r\n");
	}
	else
	{
		if(strlen(param) > 0)
		{
			swritel(conn_s, "150 Accepted data connection\r\n");
			
			u64 pos;
			u64 read = -1;
			
			Lv2FsFile fd;
			
			if(lv2FsOpen(param, LV2_O_RDONLY, &fd, 0, NULL, 0) == 0)
			{
				lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
				
			}
			else
			{
				swritel(conn_s, "452 Failed to open requested file\r\n");
			}
			
			lv2FsClose(fd);
		}
		else
		{
			swritel(conn_s, "501 Syntax error\r\n");
		}
	}
}

void cmd_stor(const char* param, int conn_s, int data_s, u32 rest)
{
	
}

void cmd_cwd(const char* param, int conn_s)
{
	
}

void cmd_nlst(const char* param, int conn_s, int data_s)
{
	
}

void cmd_list(const char* param, int conn_s, int data_s)
{
	
}

void cmd_mlsd(const char* param, int conn_s, int data_s)
{
	
}

void cmd_mlst(const char* param, int conn_s)
{
	
}

