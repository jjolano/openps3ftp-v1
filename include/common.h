#ifndef _openps3ftp_common_
#define _openps3ftp_common_

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fcntl.h>

#include "functions.h"
#include "ftpcmd.h"

#define FTPPORT		21	// port to start ftp server on (21 is standard)
#define BUFFER_SIZE	32768	// the default buffer size used in file transfers, in bytes
#define DISABLE_PASS	0	// whether or not to disable the checking of the password (1 - yes, 0 - no)

#define FD(socket) (socket & ~SOCKET_FD_MASK)
#define NIPQUAD(addr) \
         ((unsigned char *)&addr)[0], \
         ((unsigned char *)&addr)[1], \
         ((unsigned char *)&addr)[2], \
         ((unsigned char *)&addr)[3]

#define getPort(p1,p2) ((p1 * 256) + p2)

#endif /* _openps3ftp_common_ */
