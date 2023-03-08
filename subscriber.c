#include "util.h"

void usage(char* filename) {
    fprintf(stderr, "Usage: %s <ID> <IP> <PORT>\n", filename);
    exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 4) usage(argv[0]);

	// initialisation
	int sockfd, ret;
	struct sockaddr_in serv_addr;
	char buffer[sizeof(struct message)];
    memset(buffer, 0, sizeof(struct message));
	fd_set read_fds, tmp_fds;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	int one = 1;

	// disable Nagle's algorithm
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// send client IP and connection port to server
    memcpy(buffer, argv[1], CLIENT_MAX_MSG_LEN);
    tcp_send(sockfd, buffer, CLIENT_MAX_MSG_LEN);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	uint8_t power;
	uint8_t sign;
	uint16_t short_number;
	uint32_t number;
	
	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// read from stdin
			memset(buffer, 0, CLIENT_MAX_MSG_LEN); 
			read(STDIN_FILENO, buffer, CLIENT_MAX_MSG_LEN - 1);
			buffer[strcspn(buffer, "\n")] = 0;

			// send exit command to server, await response
			if (!strcmp(buffer, "exit")) {
                tcp_send(sockfd, buffer, CLIENT_MAX_MSG_LEN);
			} else if (!strncmp(buffer, "subscribe", 9) ||
						!strncmp(buffer, "unsubscribe", 11)) {
				tcp_send(sockfd, buffer, CLIENT_MAX_MSG_LEN);
			}
		}

		if (FD_ISSET(sockfd, &tmp_fds)) {
			// got info from network socket
			memset(buffer, 0, sizeof(struct message));
			tcp_recv(sockfd, buffer);

            if (!strcmp(buffer, "exit")) {
				break;
			} else if (!strcmp(buffer, "ssuccess")) {
				// print message if received ack
				printf("Subscribed to topic.\n");
			} else if (!strcmp(buffer, "usuccess")) {
				printf("Unsubscribed from topic.\n");
			} else {
				struct message *message = (struct message*)buffer;
				switch (message->type) {
				// datatype 0
				case 0:
					// store sign byte to variable
					memcpy(&sign, message->payload, sizeof(uint8_t));
					// store number
					memcpy(&number, message->payload + sizeof(uint8_t),
							sizeof(uint32_t));
					// convert to client endianess
					number = ntohl(number);
					// print signed
					if (sign) printf("%s:%u - %s - INT - -%u\n",
									inet_ntoa(message->source_ip),
									message->source_port, message->topic,
									number);
					else
						// print unsigned
						printf("%s:%u - %s - INT - %u\n",
								inet_ntoa(message->source_ip),
								message->source_port, message->topic, number);
					break;
				// datatype 1
				case 1:
					memcpy(&short_number, message->payload, sizeof(uint16_t));
					short_number = ntohs(short_number);
					printf("%s:%u - %s - SHORT_REAL - %.2f\n",
							inet_ntoa(message->source_ip),
							message->source_port, message->topic,
							(double)short_number / 100);
					break;
				// datatype 2
				case 2:
					memcpy(&sign, message->payload, sizeof(uint8_t));
					memcpy(&number, message->payload + sizeof(uint8_t),
							sizeof(uint32_t));
					memcpy(&power,
						message->payload + sizeof(uint8_t) + sizeof(uint32_t),
						sizeof(uint8_t));
					number = ntohl(number);
					if (sign) printf("%s:%u - %s - FLOAT - -%.4f\n",
										inet_ntoa(message->source_ip),
										message->source_port,
										message->topic,
										(double)number / pow(10, power));
					else
						printf("%s:%u - %s - FLOAT - %.4f\n",
								inet_ntoa(message->source_ip),
								message->source_port,
								message->topic,
								(double)number / pow(10, power));
					break;
				case 3:
					printf("%s:%u - %s - STRING - %s\n",
							inet_ntoa(message->source_ip),
							message->source_port, message->topic,
							message->payload);
					break;
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
