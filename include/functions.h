#ifndef _openps3ftp_
#define _openps3ftp_

#define exists(path) lv2FsStat(path, NULL)

void absPath(char* absPath, const char* path, const char* cwd);
//int exists(const char* path);
int isDir(const char* path);

void stoupper(char *s);
void md5(char md5[33], const char* str);

#endif /* _openps3ftp_ */
