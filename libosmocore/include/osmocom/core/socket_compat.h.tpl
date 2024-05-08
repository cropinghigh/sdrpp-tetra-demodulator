#define HAVE_STRUCT_SOCKADDR_STORAGE XX

#if HAVE_STRUCT_SOCKADDR_STORAGE
	#include <sys/socket.h>
#else
struct sockaddr_storage {
	unsigned short ss_family;
	char __data[128 - sizeof(unsigned short)];
};
#endif
