#ifndef _openps3ftp_
#define _openps3ftp_

void absPath(char* absPath, const char* path, const char* cwd);
int exists(char* path);
int isDir(char* path);

void stoupper(char *s);

#endif /* _openps3ftp_ */
