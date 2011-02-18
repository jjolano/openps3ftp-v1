#ifndef _openps3ftp_functions_
#define _openps3ftp_functions_

void absPath(char* absPath, const char* path, const char* cwd);
int exists(const char* path);
int isDir(const char* path);

//void stoupper(char *s);
void md5(char md5[33], const char* str);

#endif /* _openps3ftp_functions_ */
