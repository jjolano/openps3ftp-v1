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

const char* VERSION = "1.4";	// used in the welcome message and displayed on-screen

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
#include "ftpcmd.h"

// default login details
#define D_USER		"root"
#define D_PASS_MD5	"ab5b3a8c09da585c175de3e137424ee0" // md5("openbox") = ab5b3a8c09da585c175de3e137424ee0

char pass_md5[33];
char ipaddr[16];

static char *client_cmds[] =
{
	"USER", "PASS", "QUIT", "PASV", "PORT", "SITE", "FEAT",
	"TYPE", "REST", "RETR", "PWD", "CWD", "CDUP", "NLST",
	"LIST", "STOR", "NOOP", "DELE", "MKD", "RMD", "RNFR",
	"RNTO", "SIZE", "SYST", "HELP", "PASSWD", "MLSD", "MLST",
	"EXITAPP", "TEST"
};

static char *feat_cmds[] =
{
	"PASV", "SIZE", "REST STREAM", "SITE CHMOD", "PASSWD",
	"MLSD", "MLST type*;size*;modify*;UNIX.mode*;UNIX.uid*;UNIX.gid*;",
	"EXITAPP", "TEST"
};

static char *flash_mountpoints[] =
{
	"/dev_blind", "/dev_fflash", "/dev_rwflash", "/dev_Alejandro"
};

const int client_cmds_count	= sizeof(client_cmds)	/ sizeof(char *);
const int feat_cmds_count	= sizeof(feat_cmds)	/ sizeof(char *);
const int flash_mountpoints_count = sizeof(flash_mountpoints) / sizeof(char *);

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
	
	strcpy(cwd, "/"); // starting directory
	
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
					strcpy(cwd, tempcwd);
					ssend(conn_s, "250 CWD command successful\r\n");
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "CDUP") == 0)
			{

			}
			else
			if(strcasecmp(cmd, "PASV") == 0)
			{
				rest = 0;
				
				int data_ls = ssocket(1, NULL, FTPPORT);
				
			}
			else
			if(strcasecmp(cmd, "PORT") == 0)
			{
				rest = 0;
				
				
			}
			else
			if(strcasecmp(cmd, "LIST") == 0)
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
					
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
				}
			}
			else
			if(strcasecmp(cmd, "MLSD") == 0)
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
					
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
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
					rest = atoi(param + 1);
					ssend(conn_s, "350 REST command successful\r\n");
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
					ssend(conn_s, "350 RNFR command successful\r\n");
					
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
				char *param = strchr(buffer, ' ');
				
				char tempcwd[256];
				strcpy(tempcwd, cwd);
				
				if(param != NULL)
				{
					absPath(tempcwd, param + 1, cwd);
				}
				
				if(isDir(tempcwd))
				{
					
				}
				else
				{
					ssend(conn_s, "550 Cannot access directory\r\n");
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
					"REST STREAM",
					"SITE CHMOD",
					"SITE PASSWD",
					"SITE EXITAPP",
					"MLSD",
					"MLST type*;size*;modify*;UNIX.mode*;UNIX.uid*;UNIX.gid*;",
					"CDUP"
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
		
		/*
		// parse received string into array
		int parameter_count = 0;
		
		char *result = strtok(buffer, " ");
		
		strcpy(client_cmd[0], result);
		
		while(parameter_count < 7 && (result = strtok(NULL, " ")) != NULL)
		{
			parameter_count++;
			strcpy(client_cmd[parameter_count], result);
		}
		
		// identify the command
		int cmd_id;
		for(cmd_id = 0; cmd_id < client_cmds_count; cmd_id++)
		{
			if(strcasecmp(client_cmd[0], client_cmds[cmd_id]) == 0)
			{
				break;
			}
		}
		
		// execute command
		if(authd == 0)
		{
			// not logged in
			
			switch(cmd_id)
			{
				case 0: // USER
					#ifndef NOLOGIN
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						strcpy(user, client_cmd[1]);
						sprintf(buffer, "331 User %s OK. Password required\r\n", user);
						swritel(conn_s, buffer);
					}
					else
					{
						swritel(conn_s, "501 Please provide a username\r\n");
					}
					#else
					sprintf(buffer, "331 User %s OK. Password (not really) required\r\n", user);
					swritel(conn_s, buffer);
					#endif
					break;
				case 1: // PASS
					#ifndef NOLOGIN
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						// hash the password given
						char output[33];
						md5(output, client_cmd[1]);
					
						if(strcmp(user, LOGIN_USERNAME) == 0 && strcmp(output, passwd_md5) == 0)
						{
							swritel(conn_s, "230 Successful authentication\r\n");
							authd = 1;
						}
						else
						{
							swritel(conn_s, "430 Invalid username or password - \r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Invalid username or password\r\n");
					}
					#else
					swritel(conn_s, "230 Successful authentication\r\n");
					authd = 1;
					#endif
					break;
				case 2: // QUIT
					swritel(conn_s, "221 See you later\r\n");
					active = 0;
					break;
				default: swritel(conn_s, "530 You are not logged in\r\n");
			}
		}
		else
		{
			// logged in
			
			switch(cmd_id)
			{
				case 0: // USER
				case 1: // PASS
					swritel(conn_s, "530 You are already logged in\r\n");
					break;
				case 2: // QUIT
					swritel(conn_s, "221 See you later\r\n");
					active = 0;
					break;
				case 3: // PASV
					rest = 0;
				
					netSocketInfo snf;
					int ret = netGetSockInfo(conn_s, &snf, 1);
				
					if(ret >= 0 && snf.local_adr.s_addr != 0)
					{
						// assign a random port for passive mode
						srand(conn_s);
						
						int rand1 = (rand() % 251) + 4;
						int rand2 = rand() % 256;
						
						sprintf(buffer, "227 Entering Passive Mode (%u,%u,%u,%u,%i,%i)\r\n",
							(snf.local_adr.s_addr & 0xFF000000) >> 24,
							(snf.local_adr.s_addr & 0xFF0000) >> 16,
							(snf.local_adr.s_addr & 0xFF00) >> 8,
							(snf.local_adr.s_addr & 0xFF),
							rand1, rand2);
						
						struct sockaddr_in servaddr;
						memset(&servaddr, 0, sizeof(servaddr));
						servaddr.sin_family      = AF_INET;
						servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
						servaddr.sin_port        = htons((rand1 * 256) + rand2);
						
						list_s_data = socket(AF_INET, SOCK_STREAM, 0);
						netBind(list_s_data, (struct sockaddr *) &servaddr, sizeof(servaddr));
						netListen(list_s_data, 1);
						
						swritel(conn_s, buffer);
						
						if((conn_s_data = netAccept(list_s_data, NULL, NULL)) == -1)
						{
							swritel(conn_s, "550 PASV command failed\r\n");
						}
						else
						{
							datareq = 1;
						}
						
						break;
					}
		
					swritel(conn_s, "550 PASV command failed\r\n");
					break;
				case 4: // PORT
					if(parameter_count == 1)
					{
						rest = 0;
						
						char connectinfo[32];
						strcpy(connectinfo, client_cmd[1]);
						
						char data[7][4];
						int i = 0;
						
						char *result = strtok(connectinfo, ",");
	
						strcpy(data[0], result);
	
						while(i < 6 && (result = strtok(NULL, ",")) != NULL)
						{
							i++;
							strcpy(data[i], result);
						}
					
						char conn_ipaddr[16];
						sprintf(conn_ipaddr, "%s.%s.%s.%s", data[0], data[1], data[2], data[3]);
						
						struct sockaddr_in servaddr;
						memset(&servaddr, 0, sizeof(servaddr));
						servaddr.sin_family	= AF_INET;
						servaddr.sin_port	= htons((atoi(data[4]) * 256) + atoi(data[5]));
						inet_pton(AF_INET, conn_ipaddr, &servaddr.sin_addr);
						
						conn_s_data = socket(AF_INET, SOCK_STREAM, 0);
					
						if(connect(conn_s_data, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
						{
							swritel(conn_s, "200 PORT command successful\r\n");
							datareq = 1;
						}
						else
						{
							swritel(conn_s, "550 PORT command failed\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 5: // SITE
					if(strcasecmp(client_cmd[1], "CHMOD") == 0)
					{
						int i;
						for(i = 4; i <= parameter_count; i++)
						{
							strcat(client_cmd[3], " ");
							strcat(client_cmd[3], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[3], cwd);
					
						char perms[4];
						sprintf(perms, "0%i", atoi(client_cmd[2]));
						
						if(lv2FsChmod(filename, S_IFMT | strtol(perms, NULL, 8)) == 0)
						{
							swritel(conn_s, "250 File permissions successfully set\r\n");
						}
						else
						{
							swritel(conn_s, "550 Failed to set file permissions\r\n");
						}
					}
					else
					{
						swritel(conn_s, "500 Unrecognized SITE command\r\n");
					}
					break;
				case 6: // FEAT
					swritel(conn_s, "211- Extensions supported:\r\n");
				
					int i;
					for(i = 0; i < feat_cmds_count; i++)
					{
						sprintf(buffer, " %s\r\n", feat_cmds[i]);
						swritel(conn_s, buffer);
					}
				
					swritel(conn_s, "211 End.\r\n");
					break;
				case 7: // TYPE
					swritel(conn_s, "200 TYPE command successful\r\n");
					break;
				case 8: // REST
					if(parameter_count == 1)
					{
						rest = atoi(client_cmd[1]);
						datareq = 1;
						swritel(conn_s, "350 REST command successful\r\n");
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 9: // RETR
					if(parameter_count >= 1)
					{
						if(conn_s_data == -1)
						{
							swritel(conn_s, "425 No data connection\r\n");
							break;
						}
					
						swritel(conn_s, "150 Opening data connection\r\n");

						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
						
						u64 pos;
						u64 read = -1;
					
						Lv2FsFile fd;
					
						lv2FsOpen(filename, LV2_O_RDONLY, &fd, 0, NULL, 0);
						lv2FsLSeek64(fd, (s64)rest, SEEK_SET, &pos);
					
						if(fd >= 0)
						{
							while(lv2FsRead(fd, buf, BUFFER_SIZE - 1, &read) == 0 && read > 0)
							{
								netSend(conn_s_data, buf, read, 0);
							}
						
							if(read == 0)
							{
								swritel(conn_s, "226 Transfer complete\r\n");
							}
							else
							{
								swritel(conn_s, "426 Transfer failed\r\n");
							}
						}
						else
						{
							swritel(conn_s, "452 File access error\r\n");
						}
						
						lv2FsClose(fd);
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 10: // PWD
					sprintf(buffer, "257 \"%s\" is the current directory\r\n", cwd);
					swritel(conn_s, buffer);
					break;
				case 11: // CWD
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						strcpy(filename, client_cmd[1]);
						
						if(strcmp(filename, "../") == 0) // do CDUP
						{
							for(i = strlen(cwd) - 2; i > 0; i--)
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
							
							sprintf(buffer, "250 Directory change successful: %s\r\n", cwd);
							swritel(conn_s, buffer);
							break;
						}
						
						char temp_cwd[256];
						absPath(temp_cwd, filename, cwd);
					
						if(isDir(temp_cwd))
						{
							strcpy(cwd, temp_cwd);
							sprintf(buffer, "250 Directory change successful: %s\r\n", temp_cwd);
						}
						else
						{
							sprintf(buffer, "550 Could not change directory: %s\r\n", temp_cwd);
						}
					
						swritel(conn_s, buffer);
					}
					else
					{
						sprintf(buffer, "257 \"%s\" is the current directory\r\n", cwd);
						swritel(conn_s, buffer);
					}
					break;
				case 12: // CDUP
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
				
					sprintf(buffer, "250 Directory change successful: %s\r\n", cwd);
					swritel(conn_s, buffer);
					break;
				case 13: // NLST
					if(conn_s_data == -1)
					{
						swritel(conn_s, "425 No data connection\r\n");
						break;
					}
				
					swritel(conn_s, "150 Opening data connection\r\n");
				
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					}
					else
					{
						strcpy(filename, cwd);
					}
				
					if(lv2FsOpenDir(filename, &tempfd) == 0)
					{
						u64 read;
						Lv2FsDirent ent;
					
						while(lv2FsReadDir(tempfd, &ent, &read) == 0 && read != 0)
						{
							sprintf(buffer, "%s\r\n", ent.d_name);
							swritel(conn_s_data, buffer);
						}
						
						swritel(conn_s, "226 Transfer complete\r\n");
					}
					else
					{
						swritel(conn_s, "451 Cannot access directory\r\n");
					}
				
					lv2FsCloseDir(tempfd);
					break;
				case 14: // LIST
					if(conn_s_data == -1)
					{
						swritel(conn_s, "425 No data connection\r\n");
						break;
					}
				
					swritel(conn_s, "150 Opening data connection\r\n");
				
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					}
					else
					{
						strcpy(filename, cwd);
					}
				
					if(lv2FsOpenDir(filename, &tempfd) == 0)
					{
						u64 read;
						Lv2FsDirent ent;
						
						while(lv2FsReadDir(tempfd, &ent, &read) == 0 && read > 0)
						{
							sprintf(filename, "%s%s", cwd, ent.d_name);
							
							Lv2FsStat entry;
							lv2FsStat(filename, &entry);
						
							struct tm *tm;
							char timebuf[32];
							tm = localtime(&entry.st_mtime);
							strftime(timebuf, 31, "%b %d %Y", tm);
						
							sprintf(buffer, "%s%s%s%s%s%s%s%s%s%s 1 root root %lu %s %s\r\n", 
								((entry.st_mode & S_IFDIR) != 0)?"d":"-", 
								((entry.st_mode & S_IRUSR) != 0)?"r":"-",
								((entry.st_mode & S_IWUSR) != 0)?"w":"-",
								((entry.st_mode & S_IXUSR) != 0)?"x":"-",
								((entry.st_mode & S_IRGRP) != 0)?"r":"-",
								((entry.st_mode & S_IWGRP) != 0)?"w":"-",
								((entry.st_mode & S_IXGRP) != 0)?"x":"-",
								((entry.st_mode & S_IROTH) != 0)?"r":"-",
								((entry.st_mode & S_IWOTH) != 0)?"w":"-",
								((entry.st_mode & S_IXOTH) != 0)?"x":"-",
								(long unsigned int)entry.st_size, 
								timebuf, 
								ent.d_name);

							swritel(conn_s_data, buffer);
						}
						
						swritel(conn_s, "226 Transfer complete\r\n");
					}
					else
					{
						swritel(conn_s, "451 Cannot access directory\r\n");
					}
				
					lv2FsCloseDir(tempfd);
					break;
				case 15: // STOR
					if(parameter_count >= 1)
					{
						if(conn_s_data == -1)
						{
							swritel(conn_s, "425 No data connection\r\n");
							break;
						}
						
						swritel(conn_s, "150 Opening data connection\r\n");

						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
						
						u64 pos;
						u64 read = -1;
						u64 write = -1;
						
						Lv2FsFile fd;
					
						lv2FsOpen(filename, LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0);
						lv2FsChmod(filename, S_IFMT | 0666);
					
						lv2FsLSeek64(fd, (s32)rest, SEEK_SET, &pos);
						
						if(fd >= 0)
						{
							while((read = (u64)netRecv(conn_s_data, buf, BUFFER_SIZE - 1, MSG_WAITALL)) > 0)
							{
								lv2FsWrite(fd, buf, read, &write);
							
								if(write != read)
								{
									break;
								}
							}
							
							if(read == 0)
							{
								swritel(conn_s, "226 Transfer complete\r\n");
							}
							else
							{
								swritel(conn_s, "426 Transfer failed\r\n");
							}
						}
						else
						{
							swritel(conn_s, "452 File access error\r\n");
						}
						
						lv2FsClose(fd);
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 16: // NOOP
					swritel(conn_s, "200 Zzzz...\r\n");
					break;
				case 17: // DELE
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
						
						if(lv2FsUnlink(filename) == 0)
						{
							swritel(conn_s, "250 File successfully deleted\r\n");
						}
						else
						{
							swritel(conn_s, "550 Failed to delete file\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 18: // MKD
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					
						if(lv2FsMkdir(filename, 0775) == 0)
						{
							swritel(conn_s, "250 Directory successfully created\r\n");
						}
						else
						{
							swritel(conn_s, "550 Failed to create directory\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 19: // RMD
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					
						if(lv2FsRmdir(filename) == 0)
						{
							swritel(conn_s, "250 Directory successfully deleted\r\n");
						}
						else
						{
							swritel(conn_s, "550 Failed to remove directory\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 20: // RNFR
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(rnfr, client_cmd[1], cwd);
					
						if(exists(rnfr) == 0)
						{
							swritel(conn_s, "350 RNFR successful - ready for destination\r\n");
						}
						else
						{
							swritel(conn_s, "550 RNFR failed - file does not exist\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 21: // RNTO
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					
						if(lv2FsRename(rnfr, filename) == 0)
						{
							swritel(conn_s, "250 File successfully renamed\r\n");
						}
						else
						{
							swritel(conn_s, "550 Failed to rename file\r\n");
						}
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 22: // SIZE
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					
						Lv2FsStat entry;
						
						if(lv2FsStat(filename, &entry) == 0)
						{
							sprintf(buffer, "213 %lu\r\n", (long unsigned int)entry.st_size);
						}
						else
						{
							sprintf(buffer, "550 Requested file doesn't exist\r\n");
						}
					
						swritel(conn_s, buffer);
					}
					else
					{
						swritel(conn_s, "501 Syntax error\r\n");
					}
					break;
				case 23: // SYST
					swritel(conn_s, "215 UNIX Type: L8\r\n");
					break;
				case 24: // HELP
					swritel(conn_s, "214 No help for you.\r\n");
					break;
				case 25: // PASSWD
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						// hash the password given
						md5(passwd_md5, client_cmd[1]);
						
						Lv2FsFile fd;
						u64 written;
						
						lv2FsOpen("/dev_hdd0/game/OFTP00001/USRDIR/passwd", LV2_O_WRONLY | LV2_O_CREAT, &fd, 0, NULL, 0);
						lv2FsWrite(fd, passwd_md5, 32, &written);
						lv2FsClose(fd);
						
						swritel(conn_s, "200 Password successfully changed\r\n");
					}
					else
					{
						swritel(conn_s, "501 Invalid password\r\n");
					}
					break;
				case 26: // MLSD
					if(conn_s_data == -1)
					{
						swritel(conn_s, "425 No data connection\r\n");
						break;
					}
				
					swritel(conn_s, "150 Opening data connection\r\n");
				
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					}
					else
					{
						strcpy(filename, cwd);
					}
				
					if(lv2FsOpenDir(filename, &tempfd) == 0)
					{
						u64 read;
						Lv2FsDirent ent;
					
						while(lv2FsReadDir(tempfd, &ent, &read) == 0 && read != 0)
						{
							sprintf(filename, "%s%s", cwd, ent.d_name);
							
							Lv2FsStat entry;
							lv2FsStat(filename, &entry);
						
							struct tm *tm;
							char timebuf[32];
							tm = localtime(&entry.st_mtime);
							strftime(timebuf, 31, "%Y%m%d%H%M%S", tm);
						
							int permint = 0;

							permint +=	(((entry.st_mode & S_IRUSR) != 0)?400:0) +
									(((entry.st_mode & S_IWUSR) != 0)?200:0) +
									(((entry.st_mode & S_IXUSR) != 0)?100:0);
						
							permint +=	(((entry.st_mode & S_IRGRP) != 0)?40:0) +
									(((entry.st_mode & S_IWGRP) != 0)?20:0) +
									(((entry.st_mode & S_IXGRP) != 0)?10:0);
						
							permint +=	(((entry.st_mode & S_IROTH) != 0)?4:0) +
									(((entry.st_mode & S_IWOTH) != 0)?2:0) +
									(((entry.st_mode & S_IXOTH) != 0)?1:0);
						
							sprintf(buffer, "type=%s;size=%lu;modify=%s;UNIX.mode=0%i;UNIX.uid=root;UNIX.gid=root; %s\r\n", 
								((entry.st_mode & S_IFDIR) != 0)?"dir":"file", 
								(long unsigned int)entry.st_size, 
								timebuf, 
								permint,
								ent.d_name);
						
							swritel(conn_s_data, buffer);
						}
						
						swritel(conn_s, "226 Transfer complete\r\n");
					}
					else
					{
						swritel(conn_s, "501 Directory access error\r\n");
					}

					lv2FsCloseDir(tempfd);
					break;
				case 27: // MLST
					swritel(conn_s, "250- Listing directory");
				
					if(parameter_count >= 1)
					{
						int i;
						for(i = 2; i <= parameter_count; i++)
						{
							strcat(client_cmd[1], " ");
							strcat(client_cmd[1], client_cmd[i]);
						}
						
						absPath(filename, client_cmd[1], cwd);
					}
					else
					{
						strcpy(filename, cwd);
					}
					
					if(lv2FsOpenDir(filename, &tempfd) == 0)
					{
						u64 read;
						Lv2FsDirent ent;
					
						while(lv2FsReadDir(tempfd, &ent, &read) == 0 && read != 0)
						{
							sprintf(filename, "%s%s", cwd, ent.d_name);
							
							Lv2FsStat entry;
							lv2FsStat(filename, &entry);
						
							struct tm *tm;
							char timebuf[32];
							tm = localtime(&entry.st_mtime);
							strftime(timebuf, 31, "%Y%m%d%H%M%S", tm);
						
							int permint = 0;

							permint +=	(((entry.st_mode & S_IRUSR) != 0)?400:0) +
									(((entry.st_mode & S_IWUSR) != 0)?200:0) +
									(((entry.st_mode & S_IXUSR) != 0)?100:0);
						
							permint +=	(((entry.st_mode & S_IRGRP) != 0)?40:0) +
									(((entry.st_mode & S_IWGRP) != 0)?20:0) +
									(((entry.st_mode & S_IXGRP) != 0)?10:0);
						
							permint +=	(((entry.st_mode & S_IROTH) != 0)?4:0) +
									(((entry.st_mode & S_IWOTH) != 0)?2:0) +
									(((entry.st_mode & S_IXOTH) != 0)?1:0);
						
							sprintf(buffer, " type=%s;size=%lu;modify=%s;UNIX.mode=0%i;UNIX.uid=root;UNIX.gid=root; %s\r\n", 
								((entry.st_mode & S_IFDIR) != 0)?"dir":"file", 
								(long unsigned int)entry.st_size, 
								timebuf, 
								permint,
								ent.d_name);
						
							swritel(conn_s, buffer);
						}
					}
				
					swritel(conn_s, "250 End\r\n");
				
					lv2FsCloseDir(tempfd);
					break;
				case 28: // EXITAPP
					swritel(conn_s, "221 Exiting OpenPS3FTP, bye\r\n");
					exitapp = 1;
					break;
				case 29: // TEST
					swritel(conn_s, "211-Listing parameters\r\n");
					sprintf(buffer, "211-Count: %i\r\n", parameter_count);
					swritel(conn_s, buffer);
					
					int tx;
					for(tx = 0; tx <= parameter_count; tx++)
					{
						sprintf(buffer, " %i:%s\r\n", tx, client_cmd[tx]);
						swritel(conn_s, buffer);
					}
					
					swritel(conn_s, "211 End\r\n");
					break;
				default: swritel(conn_s, "500 Unrecognized command\r\n");
			}
			
			if(datareq == 1)
			{
				datareq = 0;
			}
			else
			{
				// close any active data connections
				if(conn_s_data > -1)
				{
					shutdown(conn_s_data, SHUT_RDWR);
					closesocket(conn_s_data);
					conn_s_data = -1;
				}
				
				if(list_s_data > -1)
				{
					shutdown(list_s_data, SHUT_RDWR);
					closesocket(list_s_data);
					list_s_data = -1;
				}
			}
		}*/
	}
	
	shutdown(conn_s, SHUT_RDWR);
	closesocket(conn_s);
	
	sys_ppu_thread_exit(0);
}

static void handleconnections(u64 list_s_p)
{
	int list_s = (int)list_s_p;
	int conn_s;
	
	while(exitapp == 0)
	{
		if((conn_s = netAccept(list_s, NULL, NULL)) >= 0)
		{
			sys_ppu_thread_t id;
			sys_ppu_thread_create(&id, handleclient, (u64)conn_s, 1500, BUFFER_SIZE * 2, 0, "ClientCmdHandler");
			
			usleep(100000); // this should solve some connection issues
		}
	}
	
	sys_ppu_thread_exit(0);
}

int main(int argc, const char* argv[])
{
	printf("OpenPS3FTP by @jjolano\nVersion %s\n\n", VERSION);

	sysRegisterCallback(EVENT_SLOT0, eventHandler, NULL);
	
	netInitialize();
	
	struct sockaddr_in servaddr;
	
	// grab ip
	char ipaddr[16];
	// a very bad way to get ip :(
	// todo: find another method
	
	int sip = socket(AF_INET, SOCK_STREAM, 0);
	
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family	= AF_INET;
	servaddr.sin_port	= htons(80);
	inet_pton(AF_INET, "74.125.226.115", &servaddr.sin_addr); // connect to google, lol
	
	if(connect(sip, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0)
	{
		netSocketInfo snf;
		int ret = netGetSockInfo(sip, &snf, 1);

		if(ret >= 0 && snf.local_adr.s_addr != 0)
		{
			sprintf(ipaddr, inet_ntoa(servaddr.sin_addr));
		}
		else
		{
			strcpy(ipaddr, "0.0.0.0");
		}
	}
	else
	{
		strcpy(ipaddr, "0.0.0.0");
	}
	
	shutdown(sip, 2);
	closesocket(sip);
	
	// set up socket address structure
	short int port = FTPPORT;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);
	
	// create listener socket
	int list_s = socket(AF_INET, SOCK_STREAM, 0);
	netBind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr));
	netListen(list_s, 10);
	
	sys_ppu_thread_t id;
	sys_ppu_thread_create(&id, handleconnections, (u64)list_s, 1500, 0x400, 0, "ConnectionHandler");
	
	printf("FTP active (%s:%i).\n", ipaddr, port);
	
	int x, j;
	char version[32], status[128];
	sprintf(version, "Version %s", VERSION);
	j = sprintf(status, "FTP active (%s:%i).", ipaddr, port);

	for (x = 0; x < flash_mountpoints_count; x++)
	{	
		// check if dev_flash is mounted rw - if so, print warning
		if(exists(flash_mountpoints[x]) == 0)
		{
			sprintf(&status[j], " WARNING: %s mount detected - please be careful when accessing %s!", flash_mountpoints[x], flash_mountpoints[x]);
			break;
		}
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
   		
		print(50, 50, "OpenPS3FTP by @jjolano", buffers[currentBuffer]->ptr);
		print(50, 100, version, buffers[currentBuffer]->ptr);
		print(50, 200, status, buffers[currentBuffer]->ptr);
		
		flip(currentBuffer);
		waitFlip();
		currentBuffer = !currentBuffer;
	}
	
	shutdown(list_s, 2);
	closesocket(list_s);
	
	netDeinitialize();
	
	printf("Process completed\n");
	return 0;
}

