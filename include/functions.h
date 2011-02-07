#ifndef _openps3ftp_
#define _openps3ftp_

void absPath(char* absPath, const char* path, const char* cwd);
int exists(const char* path);
int isDir(const char* path);

void stoupper(char *s);
void md5(const char* str, char md5[32]);

#endif /* _openps3ftp_ */
