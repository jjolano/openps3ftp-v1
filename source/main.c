// OpenPS3FTP v1.0
// by @jjolano

#include <psl1ght/lv2.h>
#include <psl1ght/lv2/net.h>
#include <psl1ght/lv2/filesystem.h>
#include <psl1ght/lv2/thread.h>

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>

#include <sysutil/video.h>
#include <sysmodule/sysmodule.h>
#include <lv2/process.h>
#include <rsx/gcm.h>
#include <rsx/reality.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/net.h>
#include <io/pad.h>

#include "helper.h"
#include "sconsole.h"

#define	FTPPORT	21
#define MAXCONN	15 // maximum clients

const char* LOGIN_USERNAME = "root";
const char* LOGIN_PASSWORD = "openbox";

int connections = 0; // connection count
int program_running = 1;
int list_s;
int conn_sa[MAXCONN + 1];
int conn_s_idinc = 0;

typedef struct {
	int height;
	int width;
	uint32_t *ptr;
	// Internal stuff
	uint32_t offset;
} buffer;

gcmContextData *context;
VideoResolution res;
int currentBuffer = 0;
buffer *buffers[2];

void waitFlip()
{
	// Block the PPU thread untill the previous flip operation has finished.
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

static void handleclient(u64 t)
{
	int conn_s = conn_sa[conn_s_idinc];
	int list_s_data = -1;
	int conn_s_data = -1;
	
	char	cwd[2048];
	u32	rest = 0;
	int	authd = 0;
	
	char	message[4096];
	char	login_user[32];
	char	login_pass[64];
	char	rename_from[2048];
	char	buffer[2048];
	
	strcpy(cwd, "/");
	
	connections++;
	conn_s_idinc++;
	
	while(program_running == 1)
	{
		sys_ppu_thread_yield();
		
		Readline(conn_s, buffer, 2047);
		
		if(strlen(buffer) == 0)
		{
			break;
		}
		
		buffer[strcspn(buffer, "\n")] = '\0';
		buffer[strcspn(buffer, "\r")] = '\0';
		
		if(strncmp(buffer, "USER", 4) == 0)
		{
			if(strlen(buffer) > 7)
			{
				strcpy(login_user, buffer+5);
				
				sprintf(message, "331 Username %s OK\r\n", login_user);
				Writeline(conn_s, message, strlen(message));
			}
			else
			{
				sprintf(message, "430 You must provide a username.\r\n");
				Writeline(conn_s, message, strlen(message));
			}
		}
		else if(strncmp(buffer, "PASS", 4) == 0)
		{
			if(strlen(buffer) > 7)
			{
				strcpy(login_pass, buffer+5);
				
				if(strcmp(LOGIN_USERNAME, login_user) == 0 && strcmp(LOGIN_PASSWORD, login_pass) == 0)
				{
					authd = 1;
					sprintf(message, "230 Successful authentication\r\n");
				}
				else
				{
					sprintf(message, "430 Invalid username or password\r\n");
					Writeline(conn_s, message, strlen(message));
				}
			}
			else
			{
				sprintf(message, "430 Invalid username or password\r\n");
			}
			
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "QUIT", 4) == 0 || strncmp(buffer, "BYE", 3) == 0)
		{
			break;
		}
		else if(authd == 0)
		{
			sprintf(message, "530 Not logged in\r\n");
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "FEAT", 4) == 0)
		{
			sprintf(message, "211-Extensions supported:\r\n");
			Writeline(conn_s, message, strlen(message));
			
			sprintf(message, " SIZE\r\n");
			Writeline(conn_s, message, strlen(message));
			sprintf(message, " PASV\r\n");
			Writeline(conn_s, message, strlen(message));
			
			
			sprintf(message, "211 End\r\n");
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "TYPE", 4) == 0)
		{
			sprintf(message, "200 TYPE is now %s\r\n", buffer+5);
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "PORT", 4) == 0)
		{
			char connectinfo[24];
			strcpy(connectinfo, buffer+5);
			
			char data[7][4];
			int len = strlen(connectinfo);
			int i, x = 0, y = 0;
			
			for(i = 0;i < len;i++)
			{
				if(connectinfo[i] == ',')
				{
					data[x][y] = '\0';
					x++;
					y = 0;
				}
				else
				{
					data[x][y] =  connectinfo[i];
					y++;
				}
			}
			
			char conn_ipaddr[16];
			sprintf(conn_ipaddr, "%s.%s.%s.%s", data[0], data[1], data[2], data[3]);

			int p1 = atoi(data[4]);
			int p2 = atoi(data[5]);
			
			short int conn_port = (p1 * 256) + p2;
			
			struct sockaddr_in	servaddr;
			
			netClose(conn_s_data);
			
			if((conn_s_data = netSocket(PF_INET, SOCK_STREAM, 0)) == 0)
			{
				memset(&servaddr, 0, sizeof(servaddr));
				servaddr.sin_family	= AF_INET;
				servaddr.sin_port	= htons(conn_port);
				inet_pton(AF_INET, conn_ipaddr, &servaddr.sin_addr);
				
				if(connect(conn_s_data, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
				{
					sprintf(message, "200 PORT command successful\r\n");
					Writeline(conn_s, message, strlen(message));
					continue;
				}
			}
			
			sprintf(message, "425 Internal Error\r\n");
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "PASV", 4) == 0)
		{
			rest = 0;
			netSocketInfo snf;
			
			int ret = netGetSockInfo(conn_s, &snf, 1);
			
			if(ret >= 0 && snf.local_adr.s_addr != 0)
			{
				// calculate the passive mode port
				srand((unsigned)time(NULL) + rand());
				
				int rand1 = 4 + rand() % 255;
				int rand2 = rand() % 256;
				
				short int port = (rand1 * 256) + rand2;
				
				sprintf(message, "227 Entering Passive Mode (%u,%u,%u,%u,%i,%i)\r\n",
					(snf.local_adr.s_addr & 0xFF000000) >> 24,
					(snf.local_adr.s_addr & 0xFF0000) >> 16,
					(snf.local_adr.s_addr & 0xFF00) >> 8,
					(snf.local_adr.s_addr & 0xFF),
					rand1, rand2);
				
				struct sockaddr_in	servaddr;	// socket address structure
				
				netClose(conn_s_data);
				netClose(list_s_data);
				
				if((list_s_data = netSocket(AF_INET, SOCK_STREAM, 0)) < 0)
				{
					// socket creation failed
					printf("[%d] Cannot create listening socket.\n", errno);
				}
				else
				{
					// set up socket address structure
					memset(&servaddr, 0, sizeof(servaddr));
					servaddr.sin_family      = AF_INET;
					servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
					servaddr.sin_port        = htons(port);
					
					// bind address to listener
					if(netBind(list_s_data, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
					{
						// bind failed
						printf("[%d] Cannot bind address to listening socket.\n", errno);
					}
					else
					{
						if(netListen(list_s_data, LISTENQ) < 0)
						{
							printf("[%d] Cannot start listener process.\n", errno);
						}
						else
						{
							Writeline(conn_s, message, strlen(message));
							
							if((conn_s_data = netAccept(list_s_data, NULL, NULL)) < 0)
							{
								printf("warning: failed to accept a connection\n");
							}
							else
							{
								// PASV success
								continue;
							}
						}
					}
				}
			}
		
		sprintf(message, "425 Internal Error\r\n");
		Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "SYST", 4) == 0)
		{
			sprintf(message, "215 UNIX Type: L8\r\n");
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "LIST", 4) == 0)
		{
			if(conn_s_data == -1)
			{
				sprintf(message, "425 No data connection\r\n");
				Writeline(conn_s, message, strlen(message));
				continue;
			}
			
			sprintf(message, "150 Accepted data connection\r\n");
			Writeline(conn_s, message, strlen(message));
			
			int root;
			if(lv2FsOpenDir(cwd, &root) == 0)
			{
				u64 read = -1;
				Lv2FsDirent ent;
				
				lv2FsReadDir(root, &ent, &read);
				
				char path[2048];
				
				while(read != 0)
				{
					strcpy(path, cwd);
					strcat(path, ent.d_name);
					
					struct stat entry; 
					stat(path, &entry);
		
					struct tm *tm;
					char timebuf[80];
					tm = localtime(&entry.st_mtime);
					strftime(timebuf, 80, "%b %d %Y", tm);
		
					sprintf(message, "%srw-rw-rw-   1 root  root        %lu %s %s\r\n", 
						((entry.st_mode & S_IFDIR) != 0)?"d":"-", 
						(long unsigned int)entry.st_size, 
						timebuf, 
						ent.d_name);
					
					Writeline(conn_s_data, message, strlen(message));
					
					if(lv2FsReadDir(root, &ent, &read) != 0)
					{
						break;
					}
				}
				
				lv2FsCloseDir(root);
			}
			
			sprintf(message, "226 Transfer complete\r\n");
			Writeline(conn_s, message, strlen(message));
			
			netClose(conn_s_data);
			netClose(list_s_data);
		}
		else if(strncmp(buffer, "PWD", 3) == 0)
		{
			sprintf(message, "257 \"%s\" is the current directory\r\n", cwd);
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "RETR", 4) == 0)
		{
			if(conn_s_data == -1)
			{
				sprintf(message, "425 No data connection\r\n");
				Writeline(conn_s, message, strlen(message));
				continue;
			}
			
			sprintf(message, "150 Opening data connection\r\n");
			Writeline(conn_s, message, strlen(message));
			
			char filename[2048];
			absPath(filename, buffer+5, cwd);

			char buf[32768];
			
			u64 pos;
			u64 read = -1;
			u64 write = -1;
			
			Lv2FsFile fd = -1;
			
			lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0);
			
			lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
			
			lv2FsRead(fd, buf, 32768, &read);
		
			while((int)read > 0)
			{
				write = (u64)netSend(conn_s_data, buf, read, 0);
				
				if(write != read || lv2FsRead(fd, buf, 32768, &read) != 0)
				{
					break;
				}
			}
		
			lv2FsClose(fd);
		
			if((int)read < 1)
			{
				read = write;
			}
		
			sprintf(message, "%i %s\r\n", 
				(write == read)?226:426, 
				(write == read)?"Transfer complete":"Transfer aborted");
		
			Writeline(conn_s, message, strlen(message));
			
			netClose(conn_s_data);
			netClose(list_s_data);
		}
		else if(strncmp(buffer, "CWD", 3) == 0)
		{
			if(buffer[4] == '/')
			{
				if(strlen(buffer) == 5)
				{
					strcpy(cwd, "/");
				}
				else
				{
					strcpy(cwd, buffer+4);
				}
			}
			else
			{
				strcat(cwd, buffer+4);
			}
		
			if(cwd[strlen(cwd)-1] != '/')
			{
				strcat(cwd, "/");
			}
		
			if(isDir(cwd))
			{
				sprintf(message, "250 Directory change successful: %s\r\n", cwd);
			}
			else
			{
				sprintf(message, "550 Could not change directory: %s\r\n", cwd);
			}
		
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "CDUP", 4) == 0)
		{
			sprintf(message, "250 Directory change successful: ");

			for(int i=strlen(cwd)-2; i>0; i--)
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
		
			strcat(message, cwd);
			strcat(message, "\r\n");
		
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "REST", 4) == 0)
		{
			rest = atoi(buffer+5);
			sprintf(message, "200 REST is now %i\r\n", rest);
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "DELE", 4) == 0)
		{
			char filename[2048];
			absPath(filename, buffer+5, cwd);
			
			int ret = lv2FsUnlink(filename);
			
			sprintf(message, "%i %s\r\n", 
				(ret == 0)?250:550, 
				(ret == 0)?"File successfully deleted":"Could not delete file");
			
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "STOR", 4) == 0)
		{
			if(conn_s_data == -1)
			{
				sprintf(message, "425 No data connection\r\n");
				Writeline(conn_s, message, strlen(message));
				continue;
			}

			sprintf(message, "150 Opening data connection\r\n");
			Writeline(conn_s, message, strlen(message));
		
			char path[2048];
			absPath(path, buffer+5, cwd);
		
			char buf[32768];
			
			u64 pos;
			u64 read = -1;
			u64 write = -1;
			
			Lv2FsFile fd = -1;
			
			lv2FsOpen(path, LV2_O_WRONLY | LV2_O_CREAT | LV2_O_TRUNC, &fd, 0, NULL, 0);
			lv2FsChmod(path, S_IFMT | 0666);
			
			lv2FsLSeek64(fd, 0, 0, &pos);
			
			if(fd > 0)
			{
				while((int)(read = (u64)netRecv(conn_s_data, buf, 32768, MSG_WAITALL)) > 0)
				{
					lv2FsWrite(fd, buf, read, &write);
					
					if(write != read)
					{
						break;
					}
				}
	
				if((int)read <= 0)
				{
					write = read;
				}
	
				lv2FsClose(fd);
			}
			else
			{
				write = 1;
			}
		
			sprintf(message, "%i %s\r\n", 
				(write == read)?226:426, 
				(write == read)?"Transfer complete":"Transfer aborted");
		
			Writeline(conn_s, message, strlen(message));
			
			netClose(conn_s_data);
			netClose(list_s_data);
		}
		else if(strncmp(buffer, "MKD", 3) == 0)
		{
			char filename[2048];
			absPath(filename, buffer+4, cwd);
			
			int ret = lv2FsMkdir(filename, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			
			sprintf(message, "%i %s\r\n", 
				(ret == 0)?250:550, 
				(ret == 0)?"Directory successfully created":"Could not create directory");
			
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "RMD", 3) == 0)
		{
			char filename[2048];
			absPath(filename, buffer+4, cwd);
			
			int ret = -1;
			// if the target is no directory -> error
			if(isDir(filename))
				ret = lv2FsRmdir(filename);
			
			sprintf(message, "%i %s\r\n", 
				(ret == 0)?250:550, 
				(ret == 0)?"Directory successfully deleted":"Could not delete directory");
			
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "SITE", 4) == 0)
		{
			if(strncmp(buffer+5, "CHMOD", 5) == 0)
			{
				char filename[2048];
				absPath(filename, buffer+16, cwd);
			
				int ret = exists(filename);
			
				if(ret == 0)
					ret = lv2FsChmod(filename, atoi(strndup(buffer+11, 4)));
			
				sprintf(message, "%i %s\r\n", 
					(ret == 0)?250:550, 
					(ret == 0)?"File permissions successfully set":"Failed to set file permissions");
				
				Writeline(conn_s, message, strlen(message));
			}
		}
		else if(strncmp(buffer, "RNFR", 4) == 0)
		{
			absPath(rename_from, buffer+5, cwd);
			
			// does source path exist ?
			int ret = exists(rename_from);
		
			sprintf(message, "%i %s\r\n", 
				(ret == 0)?350:550, 
				(ret == 0)?"File exists - ready for destination":"File does not exist");
		
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "RNTO", 4) == 0)
		{
			char filename[2048];
			absPath(filename, buffer+5, cwd);
		
			// does target path already exist ?
			int ret = exists(filename);
		
			// only rename if target doesn't exist
			if(ret != 0)
				ret = lv2FsRename(rename_from, filename);
		
			sprintf(message, "%i %s\r\n", 
				(ret == 0)?250:550, 
				(ret == 0)?"File successfully renamed":"Target file already exists or renaming failed");
			
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "SIZE", 4) == 0)
		{
			char filename[2048];
			absPath(filename, buffer+5, cwd);
			
			// does path exist ?
			if(exists(filename) == 0)
			{
				struct stat entry; 
				stat(filename, &entry);
		
				sprintf(message, "213 %lu\r\n", (long unsigned int)entry.st_size);
			}
			else
			{
				sprintf(message, "550 Requested file doesn't exist\r\n");
			}
		
			Writeline(conn_s, message, strlen(message));
		}
		else if(strncmp(buffer, "NOOP", 4) == 0)
		{
			sprintf(message, "200 Zzzz...\r\n");
			Writeline(conn_s, message, strlen(message));
		}
		else
		{
			sprintf(message, "502 Command not implemented\r\n");
			Writeline(conn_s, message, strlen(message));
		}
	}
	
	sprintf(message, "221 Goodbye.\r\n");
	Writeline(conn_s, message, strlen(message));
	
	netClose(conn_s_data);
	netClose(list_s_data);
	netClose(conn_s);
	
	connections--;
	
	sys_ppu_thread_exit(0);
}

static void handleconnections(u64 t)
{
	while(program_running == 1)
	{
		sys_ppu_thread_yield();

		if(conn_s_idinc == MAXCONN)
		{
			conn_s_idinc = 0;
		}
		
		if((conn_sa[conn_s_idinc] = netAccept(list_s, NULL, NULL)) < 0)
		{
			printf("warning: failed to accept a connection\n");
		}
		else
		{
			char* message;
			
			if(connections >= MAXCONN)
			{
				// drop client due to client limit
				message = "530 Maximum number of clients exceeded.\r\n";
				Writeline(conn_sa[conn_s_idinc], message, strlen(message));
				netClose(conn_sa[conn_s_idinc]);
				conn_sa[conn_s_idinc] = 0;
			}
			else
			{
				sys_ppu_thread_t id;
				char *thread_name = "ClientCmdHandler";
				
				sys_ppu_thread_create(&id, handleclient, 0x1337, 1500, 0x10000, 0, thread_name);
				
				message = "220 OpenPS3FTP by @jjolano\r\n";
				Writeline(conn_sa[conn_s_idinc], message, strlen(message));
			}
		}
	}
	
	sys_ppu_thread_exit(0);
}

int main(int argc, const char* argv[])
{
	struct sockaddr_in	servaddr;	// socket address structure
	
	init_screen();
	sconsoleInit(FONT_COLOR_BLACK, FONT_COLOR_WHITE, res.width, res.height);
	
	printf("OpenPS3FTP v1.0 by @jjolano\n\n");

	netInitialize();
	
	if((list_s = netSocket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		// socket creation failed
		printf("[%d] Cannot create listening socket.\n", errno);
	}
	else
	{
		short int port = FTPPORT;
		
		// set up socket address structure
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family      = AF_INET;
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
		servaddr.sin_port        = htons(port);
		
		// bind address to listener
		if(netBind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
		{
			// bind failed
			printf("[%d] Cannot bind address to listening socket.\n", errno);
		}
		else
		{
			if(netListen(list_s, LISTENQ) < 0)
			{
				printf("[%d] Cannot start listener process.\n", errno);
			}
			else
			{
				sys_ppu_thread_t id;
				char *thread_name = "ConnectionHandler";
				
				sys_ppu_thread_create(&id, handleconnections, 0x1337, 1500, 0x10000, 0, thread_name);
				
				printf("FTP server started on port %i.\n", FTPPORT);
			}
		}
	}
	
	PadInfo padinfo;
	PadData paddata;
	ioPadInit(7);

	int i, x, j;
	
	while(program_running == 1)
	{
		ioPadGetInfo(&padinfo);
		for(i = 0; i < MAX_PADS; i++)
		{
			if(padinfo.status[i])
			{
				ioPadGetData(i, &paddata);
				if(paddata.BTN_CROSS)
				{
					program_running = 0;
					break;
				}
			}
		}
		
		waitFlip();

		for(x = 0; x < res.height; x++)
		{
			for(j = 0; j < res.width; j++)
			{
				buffers[currentBuffer]->ptr[x * res.width + j] = FONT_COLOR_BLACK;
			}
		}
   		
		print(50, 50, "OpenPS3FTP v1.0 by @jjolano", buffers[currentBuffer]->ptr);
		print(50, 200, "FTP server is now running.", buffers[currentBuffer]->ptr);
		print(50, 300, "Press X to quit", buffers[currentBuffer]->ptr);

		flip(currentBuffer);
		currentBuffer = !currentBuffer;
	}
	
	netClose(list_s);
	netDeinitialize();
	ioPadEnd();
	printf("Process completed");
	return 0;
}

