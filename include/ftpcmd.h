// Functions specifically for socket/ftp use

#ifndef _openps3ftp_cmdfunc_
#define _openps3ftp_cmdfunc_

#include <psl1ght/lv2/filesystem.h>

typedef void (*listcb)(Lv2FsDirent *entry);

int recvline(int socket, char* str, int maxlen);
int ssend(int socket, const char* str);
int slisten(int port);
int sconnect(int *success, const char* ipaddr, int port);
void sclose(int *socket);
int recvfile(int socket, const char filename[256], int bufsize, s64 startpos);
int sendfile(int socket, const char filename[256], int bufsize, s64 startpos);
int slist(const char dir[256], void (*listcb)(Lv2FsDirent *entry));

#endif /* _openps3ftp_cmdfunc_ */
