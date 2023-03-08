#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <math.h>
#include <netinet/tcp.h>

#ifndef __UTILS_h__
#define __UTILS_h__

#define FD_START 5
#define MAX_ID_LEN 10
#define CLIENT_MAX_MSG_LEN 63
#define MAX_SERVER_MESSAGE_LEN 10
#define MAX_TOPIC_LEN 50
#define MAX_UDP_MESSAGE_LEN 1551

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

struct message {
    char topic[50];
    uint8_t type;
    char payload[1500];
	struct in_addr source_ip;
	in_port_t source_port;
};

void tcp_send(int sockfd, const void* buffer, size_t buflen);
void tcp_recv(int sockfd, void* buffer);

#endif // __UTILS_h__