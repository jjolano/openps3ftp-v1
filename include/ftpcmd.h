#ifndef _openps3ftp_commands_
#define _openps3ftp_commands_

void cmd_user(const char* param, int conn_s, char username[32]);
int cmd_pass(const char* param, int conn_s, const char* cmp);
void cmd_pasv(int conn_s, int *data_s, u32 *rest);
void cmd_port(const char* param, int conn_s, int *data_s, u32 *rest);
void cmd_site(const char* param, int conn_s);
void cmd_feat(int conn_s);
void cmd_retr(const char* param, int conn_s, int data_s, u32 rest);
void cmd_nlst(const char* param, int conn_s, int data_s);
void cmd_cwd(const char* param, int conn_s);
void cmd_list(const char* param, int conn_s, int data_s);
void cmd_stor(const char* param, int conn_s, int data_s, u32 rest);
void cmd_mlsd(const char* param, int conn_s, int data_s);
void cmd_mlst(const char* param, int conn_s);

#endif /* _openps3ftp_commands_ */
