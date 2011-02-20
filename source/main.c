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

#define FTPPORT		21	// port to start ftp server on (21 is standard)
#define BUFFER_SIZE	16384	// the default buffer size used in file transfers, in bytes

// tested buffer values (smaller buffer size allows for more connections): 
// <= 4096 - doesn't even connect
// == 8192 - works, but transfer speed is a little lesser compared to 16k or 32k
// == 16384 - works great - similar to 32768
// == 32768 - works great - similar to 16384
// >= 65536 - POS, slowest transfer EVER.

const char* VERSION = "1.4-dev";	// used in the welcome message and displayed on-screen

#include <assert.h>
#include <fcntl.h>

#include <psl1ght/lv2/filesystem.h>

#include <sysutil/video.h>
#include <sysutil/events.h>

#include <rsx/gcm.h>
#include <rsx/reality.h>

#include <net/net.h>
#include <sys/thread.h>

#include "common.h"
#include "sconsole.h"

// default login details
#define D_USER		"root"
#define D_PASS_MD5	"ab5b3a8c09da585c175de3e137424ee0" // md5("openbox") = ab5b3a8c09da585c175de3e137424ee0

char pass_md5[33];
char netstat[64];

int exitapp = 0;
int currentBuffer = 0;

typedef struct {
	int height;
	int width;
	uint32_t *ptr;
	// Internal stuff
	uint32_t offset;
} buffer;

gcmContextData *context;
VideoResolution res;
buffer *buffers[2];

void waitFlip()
{
	// Block the PPU thread until the previous flip operation has finished.
	while (gcmGetFlipStatus() != 0)
		usleep(200);
	gcmResetFlipStatus();
}

void flip(s32 buffer)
{
	assert(gcmSetFlip(context, buffer) == 0);
	realityFlushBuffer(context);
	gcmSetWaitFlip(context);
}

void makeBuffer(int id, int size)
{
	buffer *buf = malloc(sizeof(buffer));
	buf->ptr = rsxMemAlign(16, size);
	assert(buf->ptr != NULL);

	assert(realityAddressToOffset(buf->ptr, &buf->offset) == 0);
	assert(gcmSetDisplayBuffer(id, buf->offset, res.width * 4, res.width, res.height) == 0);

	buf->width = res.width;
	buf->height = res.height;
	buffers[id] = buf;
}

void init_screen()
{
	void *host_addr = memalign(1024*1024, 1024*1024);
	assert(host_addr != NULL);

	context = realityInit(0x10000, 1024*1024, host_addr); 
	assert(context != NULL);

	VideoState state;
	assert(videoGetState(0, 0, &state) == 0);
	assert(state.state == 0);

	assert(videoGetResolution(state.displayMode.resolution, &res) == 0);

	VideoConfiguration vconfig;
	memset(&vconfig, 0, sizeof(VideoConfiguration));
	vconfig.resolution = state.displayMode.resolution;
	vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
	vconfig.pitch = res.width * 4;

	assert(videoConfigure(0, &vconfig, NULL, 0) == 0);
	assert(videoGetState(0, 0, &state) == 0); 

	s32 buffer_size = 4 * res.width * res.height;

	gcmSetFlipMode(GCM_FLIP_VSYNC);
	makeBuffer(0, buffer_size);
	makeBuffer(1, buffer_size);

	gcmResetFlipStatus();
	flip(1);
}

void eventHandler(u64 status, u64 param, void * userdata)
{
	if(status == EVENT_REQUEST_EXITAPP)
	{
		exitapp = 1;
	}
}

static void handleclient(u64 conn_s_p)
{
	int conn_s = (int)conn_s_p; // main communications socket
	int data_s = -1; // data socket
	
	int connactive = 1; // whether the ftp connection is active or not
	int dataactive = 0; // prevent the data connection from being closed at the end of the loop
	int loggedin = 0; // whether the user is logged in or not
	
	char cwd[256]; // Current Working Directory
	int rest = 0; // for resuming file transfers
	
	char buffer[1024];
	
	// generate pasv output
	netSocketInfo snf;
	netGetSockInfo(conn_s, &snf, 1);
	
	srand(conn_s);
	int p1 = (rand() % 251) + 4;
	int p2 = rand() % 256;
	
	char pasv_output[16];
	sprintf(pasv_output, "%u,%u,%u,%u,%i,%i",
		(snf.local_adr.s_addr & 0xFF000000) >> 24, (snf.local_adr.s_addr & 0xFF0000) >> 16,
		(snf.local_adr.s_addr & 0xFF00) >> 8, (snf.local_adr.s_addr & 0xFF),
		p1, p2);
	
	// set working directory
	strcpy(cwd, "/");
	
	// welcome message
	ssend(conn_s, "220-OpenPS3FTP by @jjolano\r\n");
	sprintf(buffer, "220 Version %s\r\n", VERSION);
	ssend(conn_s, buffer);
	
	while(exitapp == 0 && connactive == 1 && recv(conn_s, buffer, 1023, 0) > 0)
	{
		// get rid of the newline at the end of the string
		buffer[strcspn(buffer, "\n")] = '\0';
		buffer[strcspn(buffer, "\r")] = '\0';
		
		char *cmd = strtok(buffer, " ");
		
		if(cmd == NULL)
		{
			strcpy(cmd, buffer);
		}
		
		if(loggedin)
		{
			// available commands when logged in
			if(strcasecmp(cmd, "CWD") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				char tempcwd[256];
				strcpy(tempcwd, cwd);
				
				if(param != NULL)
				{
					absPath(tempcwd, param + 1, cwd);
				}
				
				if(isDir(tempcwd))
				{
					sprintf(buffer, "250 Directory change successful: %s\r\n", tempcwd);
					ssend(conn_s, buffer);
					strcpy(cwd, tempcwd);
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "CDUP") == 0)
			{
				sprintf(buffer, "250 Directory change successful: %s\r\n", cwd);
				ssend(conn_s, buffer);
				
				for(int i = strlen(cwd) - 2; i > 0; i--)
				{
					if(cwd[i] != '/')
					{
						cwd[i] = '\0';
					}
					else
					{
						break;
					}
				}
			}
			else
			if(strcasecmp(cmd, "PASV") == 0)
			{
				rest = 0;
				
				int data_ls = ssocket(1, NULL, FTPPORT);
				
				if(data_ls > 0)
				{
					sprintf(buffer, "227 Entering Passive Mode (%s)\r\n", pasv_output);
					ssend(conn_s, buffer);
					
					data_s = accept(data_ls, NULL, NULL);
					
					sclose(&data_ls);
					
					if(data_s > 0)
					{
						dataactive = 1;
					}
					else
					{
						ssend(conn_s, "451 Data connection failed\r\n");
					}
				}
				else
				{
					ssend(conn_s, "451 Cannot create data socket\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "PORT") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					rest = 0;
					
					char data[6][4];
					char *splitstr = strtok(param + 1, ",");
					
					int i = 0;
					while(i < 6 && splitstr != NULL)
					{
						strcpy(data[i++], splitstr);
						splitstr = strtok(NULL, ",");
					}
					
					if(i == 6)
					{
						char ipaddr[16];
						sprintf(ipaddr, "%s.%s.%s.%s", data[0], data[1], data[2], data[3]);
						
						data_s = ssocket(0, ipaddr, ((atoi(data[4]) * 256) + atoi(data[5])));
						
						if(data_s == 0)
						{
							ssend(conn_s, "200 PORT command successful\r\n");
							dataactive = 1;
						}
						else
						{
							ssend(conn_s, "451 Data connection failed\r\n");
						}
					}
					else
					{
						ssend(conn_s, "501 Insufficient connection info\r\n");
					}
				}
				else
				{
					ssend(conn_s, "501 No connection info given\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "LIST") == 0)
			{
				if(data_s > 0)
				{
					char *param = strchr(buffer, ' ');
					
					char tempcwd[256];
					strcpy(tempcwd, cwd);
					
					if(param != NULL)
					{
						if(strncmp(param + 1, "-a", 2) != 0) // gFTP compatibility
						{
							absPath(tempcwd, param + 1, cwd);
						}
					}
					
					if(isDir(tempcwd))
					{
						void listcb(Lv2FsDirent *entry)
						{
							char filename[256];
							absPath(filename, entry->d_name, cwd);
							
							Lv2FsStat buf;
							lv2FsStat(filename, &buf);
							
							char timebuf[16];
							strftime(timebuf, 15, "%Y-%m-%d %H:%M", localtime(&buf.st_mtime));
							
							sprintf(buffer, "%s%s%s%s%s%s%s%s%s%s 1 root root %i %s %s\r\n",
								((buf.st_mode & S_IFDIR) != 0) ? "d" : "-", 
								((buf.st_mode & S_IRUSR) != 0) ? "r" : "-",
								((buf.st_mode & S_IWUSR) != 0) ? "w" : "-",
								((buf.st_mode & S_IXUSR) != 0) ? "x" : "-",
								((buf.st_mode & S_IRGRP) != 0) ? "r" : "-",
								((buf.st_mode & S_IWGRP) != 0) ? "w" : "-",
								((buf.st_mode & S_IXGRP) != 0) ? "x" : "-",
								((buf.st_mode & S_IROTH) != 0) ? "r" : "-",
								((buf.st_mode & S_IWOTH) != 0) ? "w" : "-",
								((buf.st_mode & S_IXOTH) != 0) ? "x" : "-",
								(int)buf.st_size, timebuf, entry->d_name);
							
							ssend(data_s, buffer);
						}
						
						slist(tempcwd, listcb);
					}
					else
					{
						ssend(conn_s, "550 Cannot access directory\r\n");
					}
				}
				else
				{
					ssend(conn_s, "425 No data connection\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "MLSD") == 0)
			{
				if(data_s > 0)
				{
					char *param = strchr(buffer, ' ');
					
					char tempcwd[256];
					strcpy(tempcwd, cwd);
					
					if(param != NULL)
					{
						absPath(tempcwd, param + 1, cwd);
					}
					
					if(isDir(tempcwd))
					{
						void listcb(Lv2FsDirent *entry)
						{
							char filename[256];
							absPath(filename, entry->d_name, cwd);
							
							Lv2FsStat buf;
							lv2FsStat(filename, &buf);
							
							char timebuf[16];
							strftime(timebuf, 15, "%Y%m%d%H%M%S", localtime(&buf.st_mtime));
							
							char dirtype[2];
							if(strcmp(entry->d_name, ".") == 0)
							{
								strcpy(dirtype, "c");
							}
							else
							if(strcmp(entry->d_name, "..") == 0)
							{
								strcpy(dirtype, "p");
							}
							
							sprintf(buffer, "type=%s%s;size=%i;modify=%s;UNIX.mode=0%i%i%i;UNIX.uid=root;UNIX.gid=root; %s\r\n",
								dirtype, ((buf.st_mode & S_IFDIR) != 0) ? "dir" : "file", (int)buf.st_size, timebuf,
								(((buf.st_mode & S_IRUSR) != 0) * 4 +
								((buf.st_mode & S_IWUSR) != 0) * 2 +
								((buf.st_mode & S_IXUSR) != 0) * 1),
								(((buf.st_mode & S_IRGRP) != 0) * 4 +
								((buf.st_mode & S_IWGRP) != 0) * 2 +
								((buf.st_mode & S_IXGRP) != 0) * 1),
								(((buf.st_mode & S_IROTH) != 0) * 4 +
								((buf.st_mode & S_IWOTH) != 0) * 2 +
								((buf.st_mode & S_IXOTH) != 0) * 1),
								entry->d_name);
							
							ssend(data_s, buffer);
						}
						
						slist(tempcwd, listcb);
					}
					else
					{
						ssend(conn_s, "550 Cannot access directory\r\n");
					}
				}
				else
				{
					ssend(conn_s, "425 No data connection\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "STOR") == 0)
			{
				if(data_s > 0)
				{
					char *param = strchr(buffer, ' ');
					
					if(param != NULL)
					{
						char filename[256];
						absPath(filename, param + 1, cwd);
						
						ssend(conn_s, "150 Accepted data connection\r\n");
						
						if(recvfile(data_s, filename, BUFFER_SIZE, (s64)rest) == 0)
						{
							ssend(conn_s, "226 Transfer complete\r\n");
						}
						else
						{
							ssend(conn_s, "451 Transfer failed\r\n");
						}
					}
					else
					{
						ssend(conn_s, "501 No file specified\r\n");
					}
				}
				else
				{
					ssend(conn_s, "425 No data connection\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "RETR") == 0)
			{
				if(data_s > 0)
				{
					char *param = strchr(buffer, ' ');
					
					if(param != NULL)
					{
						char filename[256];
						absPath(filename, param + 1, cwd);
						
						if(exists(filename) == 0)
						{
							ssend(conn_s, "150 Accepted data connection\r\n");
							
							if(sendfile(data_s, filename, BUFFER_SIZE, (s64)rest) == 0)
							{
								ssend(conn_s, "226 Transfer complete\r\n");
							}
							else
							{
								ssend(conn_s, "451 Transfer failed\r\n");
							}
						}
						else
						{
							ssend(conn_s, "550 File does not exist\r\n");
						}
					}
					else
					{
						ssend(conn_s, "501 No file specified\r\n");
					}
				}
				else
				{
					ssend(conn_s, "425 No data connection\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "PWD") == 0)
			{
				sprintf(buffer, "257 \"%s\" is the current directory\r\n", cwd);
				ssend(conn_s, buffer);
			}
			else
			if(strcasecmp(cmd, "TYPE") == 0)
			{
				ssend(conn_s, "200 TYPE command successful\r\n");
			}
			else
			if(strcasecmp(cmd, "REST") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					ssend(conn_s, "350 REST command successful\r\n");
					rest = atoi(param + 1);
					dataactive = 1;
				}
				else
				{
					ssend(conn_s, "501 No restart point\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "DELE") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					char filename[256];
					absPath(filename, param + 1, cwd);
					
					if(lv2FsUnlink(filename) == 0)
					{
						ssend(conn_s, "250 File successfully deleted\r\n");
					}
					else
					{
						ssend(conn_s, "550 Cannot delete file\r\n");
					}
				}
				else
				{
					ssend(conn_s, "501 No filename specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "MKD") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					char filename[256];
					absPath(filename, param + 1, cwd);
					
					if(lv2FsMkdir(filename, 0755) == 0)
					{
						sprintf(buffer, "257 \"%s\" was successfully created\r\n", param);
						ssend(conn_s, buffer);
					}
					else
					{
						ssend(conn_s, "550 Cannot create directory\r\n");
					}
				}
				else
				{
					ssend(conn_s, "501 No filename specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "RMD") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					char filename[256];
					absPath(filename, param + 1, cwd);
					
					if(lv2FsRmdir(filename) == 0)
					{
						ssend(conn_s, "250 Directory was successfully removed\r\n");
					}
					else
					{
						ssend(conn_s, "550 Cannot remove directory\r\n");
					}
				}
				else
				{
					ssend(conn_s, "501 No filename specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "RNFR") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					ssend(conn_s, "350 RNFR accepted - ready for destination\r\n");
					
					if(recv(conn_s, buffer, 1023, 0) > 0)
					{
						cmd = strtok(buffer, " ");
						
						if(cmd == NULL)
						{
							strcpy(cmd, buffer);
						}
						
						if(strcasecmp(cmd, "RNTO") == 0)
						{
							char *param2 = strchr(buffer, ' ');
							
							if(param2 != NULL)
							{
								char rnfr[256], rnto[256];
								absPath(rnfr, param + 1, cwd);
								absPath(rnto, param2 + 1, cwd);
								
								if(lv2FsRename(rnfr, rnto) == 0)
								{
									ssend(conn_s, "250 File was successfully renamed or moved\r\n");
								}
								else
								{
									ssend(conn_s, "550 Cannot rename or move file\r\n");
								}
							}
							else
							{
								ssend(conn_s, "501 No file specified\r\n");
							}
						}
						else
						{
							ssend(conn_s, "503 Bad command sequence\r\n");
						}
					}
					else
					{
						// error in recv, disconnect
						connactive = 0;
					}
				}
				else
				{
					ssend(conn_s, "501 No file specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "SITE") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					cmd = strtok(param + 1, " ");
					
					if(cmd == NULL)
					{
						strcpy(cmd, param + 1);
					}
					
					if(strcasecmp(cmd, "CHMOD") == 0)
					{
						char *param2 = strchr(param + 1, ' ');
						
						if(param2 != NULL)
						{
							char *temp = strtok(param2 + 1, " ");
							char *filename = strchr(param2 + 1, ' ');
							
							if(temp != NULL && filename != NULL)
							{
								char perms[4];
						
								if(strlen(temp) == 4)
								{
									strcpy(perms, temp);
								}
								else
								{
									sprintf(perms, "0%s", temp);
								}
		
								if(lv2FsChmod(filename + 1, S_IFMT | strtol(perms, NULL, 8)) == 0)
								{
									ssend(conn_s, "250 File permissions successfully set\r\n");
								}
								else
								{
									ssend(conn_s, "550 Cannot set file permissions\r\n");
								}
							}
							else
							{
								ssend(conn_s, "501 Not enough parameters\r\n");
							}
						}
						else
						{
							ssend(conn_s, "501 No parameters given\r\n");
						}
					}
					else
					if(strcasecmp(cmd, "HELP") == 0)
					{
						ssend(conn_s, "214-Special OpenPS3FTP commands:\r\n");
						ssend(conn_s, " SITE PASSWD <newpassword> - Change your password\r\n");
						ssend(conn_s, " SITE EXITAPP - Remotely quit OpenPS3FTP\r\n");
						ssend(conn_s, " SITE HELP - Show this message\r\n");
						ssend(conn_s, "214 End\r\n");
					}
					else
					if(strcasecmp(cmd, "PASSWD") == 0)
					{
						char *param2 = strchr(param + 1, ' ');
						
						if(param2 != NULL)
						{
							char md5pass[33];
							md5(md5pass, param2 + 1);
							
							Lv2FsFile fd;
							u64 written;
							
							if(lv2FsOpen("/dev_hdd0/game/OFTP00001/USRDIR/passwd", LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0) == 0)
							{
								lv2FsWrite(fd, md5pass, 32, &written);
								ssend(conn_s, "200 FTP password successfully changed\r\n");
							}
							else
							{
								ssend(conn_s, "550 Cannot change FTP password\r\n");
							}
						
							lv2FsClose(fd);
						}
						else
						{
							ssend(conn_s, "501 No password given\r\n");
						}
					}
					else
					if(strcasecmp(cmd, "EXITAPP") == 0)
					{
						ssend(conn_s, "221 Exiting OpenPS3FTP\r\n");
						exitapp = 1;
					}
				}
				else
				{
					ssend(conn_s, "501 No SITE command specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "NOOP") == 0)
			{
				ssend(conn_s, "200 NOOP command successful\r\n");
			}
			else
			if(strcasecmp(cmd, "NLST") == 0)
			{
				if(data_s > 0)
				{
					char *param = strchr(buffer, ' ');
					
					char tempcwd[256];
					strcpy(tempcwd, cwd);
					
					if(param != NULL)
					{
						absPath(tempcwd, param + 1, cwd);
					}
					
					if(isDir(tempcwd))
					{
						void listcb(Lv2FsDirent *entry)
						{
							ssend(data_s, entry->d_name);
						}
						
						slist(tempcwd, listcb);
					}
					else
					{
						ssend(conn_s, "550 Cannot access directory\r\n");
					}
				}
				else
				{
					ssend(conn_s, "425 No data connection\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "MLST") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				char tempcwd[256];
				strcpy(tempcwd, cwd);
				
				if(param != NULL)
				{
					absPath(tempcwd, param + 1, cwd);
				}
				
				if(isDir(tempcwd))
				{
					void listcb(Lv2FsDirent *entry)
					{
						char filename[256];
						absPath(filename, entry->d_name, cwd);
						
						Lv2FsStat buf;
						lv2FsStat(filename, &buf);
						
						char timebuf[16];
						strftime(timebuf, 15, "%Y%m%d%H%M%S", localtime(&buf.st_mtime));
						
						char dirtype[2];
						if(strcmp(entry->d_name, ".") == 0)
						{
							strcpy(dirtype, "c");
						}
						else
						if(strcmp(entry->d_name, "..") == 0)
						{
							strcpy(dirtype, "p");
						}
						
						sprintf(buffer, " type=%s%s;size=%i;modify=%s;UNIX.mode=0%i%i%i;UNIX.uid=root;UNIX.gid=root; %s\r\n",
							dirtype, ((buf.st_mode & S_IFDIR) != 0) ? "dir" : "file", (int)buf.st_size, timebuf,
							(((buf.st_mode & S_IRUSR) != 0) * 4 +
							((buf.st_mode & S_IWUSR) != 0) * 2 +
							((buf.st_mode & S_IXUSR) != 0) * 1),
							(((buf.st_mode & S_IRGRP) != 0) * 4 +
							((buf.st_mode & S_IWGRP) != 0) * 2 +
							((buf.st_mode & S_IXGRP) != 0) * 1),
							(((buf.st_mode & S_IROTH) != 0) * 4 +
							((buf.st_mode & S_IWOTH) != 0) * 2 +
							((buf.st_mode & S_IXOTH) != 0) * 1),
							entry->d_name);
						
						ssend(conn_s, buffer);
					}
					
					slist(tempcwd, listcb);
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "BYE") == 0)
			{
				ssend(conn_s, "221 Bye!\r\n");
				connactive = 0;
			}
			else
			if(strcasecmp(cmd, "FEAT") == 0)
			{
				ssend(conn_s, "211-Extensions supported:\r\n");
				
				static char *feat_cmds[] =
				{
					"PASV",
					"PORT",
					"SIZE",
					"CDUP",
					"MLSD",
					"MLST type*;size*;modify*;UNIX.mode*;UNIX.uid*;UNIX.gid*;",
					"REST STREAM",
					"SITE CHMOD",
					"SITE PASSWD",
					"SITE EXITAPP"
				};
				
				const int feat_cmds_count = sizeof(feat_cmds) / sizeof(char *);
				
				for(int i = 0; i < feat_cmds_count; i++)
				{
					sprintf(buffer, " %s\r\n", feat_cmds[i]);
					ssend(conn_s, buffer);
				}
				
				ssend(conn_s, "211 End\r\n");
			}
			else
			if(strcasecmp(cmd, "SIZE") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					char filename[256];
					absPath(filename, param + 1, cwd);
					
					Lv2FsStat buf;
					if(lv2FsStat(filename, &buf) == 0)
					{
						sprintf(buffer, "%i %i", ((buf.st_mode & S_IFDIR) != 0) ? 212 : 213, (int)buf.st_size);
						ssend(conn_s, buffer);
					}
					else
					{
						ssend(conn_s, "550 File does not exist\r\n");
					}
				}
				else
				{
					ssend(conn_s, "501 No file specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "SYST") == 0)
			{
				ssend(conn_s, "215 UNIX Type: L8\r\n");
			}
			else
			if(strcasecmp(cmd, "USER") == 0 || strcasecmp(cmd, "PASS") == 0)
			{
				ssend(conn_s, "230 You are already logged in\r\n");
			}
			else
			{
				sprintf(buffer, "500 Unrecognized command: \"%s\"\r\n", cmd);
				ssend(conn_s, buffer);
			}
			
			if(dataactive == 1)
			{
				dataactive = 0;
			}
			else
			{
				sclose(&data_s);
			}
		}
		else
		{
			// available commands when not logged in
			if(strcasecmp(cmd, "USER") == 0)
			{
				char *param = strchr(buffer, ' ');
				
				if(param != NULL)
				{
					sprintf(buffer, "331 User %s OK. Password required\r\n", param);
					ssend(conn_s, buffer);
					
					if(recv(conn_s, buffer, 1023, 0) > 0)
					{
						cmd = strtok(buffer, " ");
						
						if(cmd == NULL)
						{
							strcpy(cmd, buffer);
						}
						
						if(strcasecmp(cmd, "PASS") == 0)
						{
							char *param2 = strchr(buffer, ' ');
							
							if(param2 != NULL)
							{
								char userpass_md5[33];
								md5(userpass_md5, param2 + 1);
								
								if(strcmp(D_USER, param + 1) == 0 && strcmp(D_PASS_MD5, userpass_md5) == 0)
								{
									ssend(conn_s, "230 Welcome to your PS3!\r\n");
									loggedin = 1;
								}
								else
								{
									ssend(conn_s, "430 Invalid username or password\r\n");
								}
							}
							else
							{
								ssend(conn_s, "501 No password given\r\n");
							}
						}
						else
						{
							ssend(conn_s, "503 Bad command sequence\r\n");
						}
					}
					else
					{
						// error in recv, disconnect
						connactive = 0;
					}
				}
				else
				{
					ssend(conn_s, "501 No user specified\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "QUIT") == 0 || strcasecmp(cmd, "BYE") == 0)
			{
				ssend(conn_s, "221 Bye!\r\n");
				connactive = 0;
			}
			else
			{
				ssend(conn_s, "530 Not logged in\r\n");
			}
		}
	}
	
	sclose(&conn_s);
	sclose(&data_s);
	
	sys_ppu_thread_exit(0);
}

static void handleconnections(u64 list_s_p)
{
	int list_s = (int)list_s_p;
	int conn_s;
	
	while(exitapp == 0)
	{
		if((conn_s = netAccept(list_s, NULL, NULL)) > 0)
		{
			sys_ppu_thread_t id;
			sys_ppu_thread_create(&id, handleclient, (u64)conn_s, 1500, BUFFER_SIZE * 2, 0, "ClientCmdHandler");
			
			usleep(100000); // this should solve some connection issues
		}
	}
	
	sys_ppu_thread_exit(0);
}

static void ipaddr_get(u64 unused)
{
	// temporary method until something new comes up
	// will work only if internet connection is available
	int conn_s = ssocket(0, "8.8.8.8", 53);
	
	if(conn_s > 0)
	{
		netSocketInfo snf;
		netGetSockInfo(conn_s, &snf, 1);
		
		sprintf(netstat, "Active; IP: %u.%u.%u.%u Port: %i",
			(snf.local_adr.s_addr & 0xFF000000) >> 24, (snf.local_adr.s_addr & 0xFF0000) >> 16,
			(snf.local_adr.s_addr & 0xFF00) >> 8, (snf.local_adr.s_addr & 0xFF),
			FTPPORT);
		
		sclose(&conn_s);
	}
	else
	{
		strcpy(netstat, "Active; IP Retrieval Failed");
	}
	
	sys_ppu_thread_exit(0);
}

int main(int argc, const char* argv[])
{
	sysRegisterCallback(EVENT_SLOT0, eventHandler, NULL);
	netInitialize();
	
	char version[16], status[128];
	int x, j, rwflashmount = 0;
	
	int list_s = ssocket(1, NULL, FTPPORT);
	
	if(list_s > 0)
	{
		// start connection handler
		sys_ppu_thread_t id;
		sys_ppu_thread_create(&id, handleconnections, (u64)list_s, 1500, 0x400, 0, "ConnectionHandler");
		
		// try to get the ip address
		sys_ppu_thread_t id2;
		sys_ppu_thread_create(&id2, ipaddr_get, 0, 1500, 0x400, 0, "GetIPAddress");
		
		strcpy(netstat, "Active");
	}
	
	// check if dev_flash is mounted rw
	if(exists("/dev_blind") == 0 || exists("/dev_rwflash") == 0 || exists("/dev_fflash") == 0 || exists("/dev_Alejandro") == 0)
	{
		rwflashmount = 1;
	}
	
	// load password file
	if(exists("/dev_hdd0/game/OFTP00001/USRDIR/passwd") == 0)
	{
		Lv2FsFile fd;
		u64 read;
		
		lv2FsOpen("/dev_hdd0/game/OFTP00001/USRDIR/passwd", LV2_O_RDONLY, &fd, 0, NULL, 0);
		lv2FsRead(fd, pass_md5, 32, &read);
		lv2FsClose(fd);
	}
	
	if(strlen(pass_md5) != 32)
	{
		strcpy(pass_md5, D_PASS_MD5);
	}
	
	init_screen();
	sconsoleInit(FONT_COLOR_BLACK, FONT_COLOR_GREEN, res.width, res.height);
	
	waitFlip();
	
	while(exitapp == 0)
	{
		sysCheckCallback();
		
		for(x = 0; x < res.height; x++)
		{
			for(j = 0; j < res.width; j++)
			{
				buffers[currentBuffer]->ptr[x * res.width + j] = FONT_COLOR_BLACK;
			}
		}
   		
		print(50, 50, "OpenPS3FTP by jjolano (Twitter: @jjolano)", buffers[currentBuffer]->ptr);
		print(50, 90, version, buffers[currentBuffer]->ptr);
		print(50, 150, "Note: IP address retrieval is experimental - you can always find your console's IP address by navigating to Network Settings -> Status.", buffers[currentBuffer]->ptr);
		
		sprintf(status, "Status: %s", netstat);
		print(50, 250, status, buffers[currentBuffer]->ptr);
		
		if(rwflashmount == 1)
		{
			print(50, 350, "Warning: A _writable_ mountpoint that points to dev_flash was detected. Please exercise caution when browsing through any of these directories: /dev_blind /dev_rwflash /dev_fflash /dev_Alejandro", buffers[currentBuffer]->ptr);
		}
		
		flip(currentBuffer);
		waitFlip();
		currentBuffer = !currentBuffer;
	}
	
	sclose(&list_s);
	
	netDeinitialize();
	sleep(1); // allow any active connections to disconnect
	
	return 0;
}

