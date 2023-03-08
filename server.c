#include "server.h"
#include "util.h"

void usage(char* filename) {
    fprintf(stderr, "Usage: %s <PORT>\n", filename);
    exit(0);
}

struct tcp_client* init_client(char* id) {
    struct tcp_client *res = malloc(sizeof(struct tcp_client));
    if (!res) return NULL;
    res->id = strdup(id);
    res->sockfd = 0;
    res->nr_topics = 0;
    res->nr_messages = 0;
    res->topics = NULL;
    return res;
}

int add_client(struct tcp_client** clients, int* nr_clients,
                struct tcp_client* new_client, int sockfd) {
    int n = *nr_clients;
    for (int i = 0; i < n; i++) {
        // check if client id is already known
        if (!strcmp((*clients)[i].id, new_client->id)) {
            // check if client is connected
            if ((*clients)[i].sockfd == 0) {
                (*clients)[i].sockfd = sockfd;
                free(new_client);
                return 1;
            }
            new_client->sockfd = sockfd;
            return 0; // client should be rejected
        }
    }
    new_client->sockfd = sockfd; // set sockfd
    // allocate memory in array for the new client
    (*clients) = realloc((*clients), (n + 1) * sizeof(struct tcp_client));
    DIE((*clients) == NULL, "realloc new client");
    // save client's information
	memcpy(&(*clients)[n], new_client, sizeof(struct tcp_client));
    free(new_client); // free aux memory
    *nr_clients = n + 1; // increment number of clients
	return 1; // success
}

void rm_client(struct tcp_client** clients, char* id, int* nr_clients) {
    int idx = get_client_by_id(*clients, id, *nr_clients);
    if (idx == -1) return;
    free((*clients)[idx].id);

    *nr_clients = *nr_clients - 1;

    // overwrite memory, so other clients are stored well in array
    memcpy(&((*clients)[idx]), &(*clients)[idx + 1], (*nr_clients - idx) * sizeof(struct tcp_client));

    
    // reallocate memory
    (*clients) = realloc((*clients), *nr_clients * sizeof(struct tcp_client));
}

void disconnect_client(struct tcp_client* client, struct tcp_client** clients,
                        int* nr_clients, int refuse) {
    char buffer[] = "exit";
    // send exit message to client
    tcp_send(client->sockfd, buffer, MAX_SERVER_MESSAGE_LEN);

    // close sockfd
    close(client->sockfd);
    client->sockfd = 0;

    /*
        rm_clients removes client from array, the refuse flag is used to
        delimitate whenever the client to be disconnected should be removed
        from array or not
    */
    if (client->nr_topics == 0 && !refuse)
        rm_client(clients, client->id, nr_clients);
}

void disconnect_all(struct tcp_client* clients, int nr_clients) {
    char exit_code[] = "exit";
    for (int i = 0; i < nr_clients; i++) {
        int sockfd = clients[i].sockfd;
        if (sockfd != 0) {
            tcp_send(sockfd, exit_code, MAX_SERVER_MESSAGE_LEN);
        }
    }
}

int get_client_by_id(struct tcp_client* clients, char* id, int nr_clients) {
    for (int i = 0; i < nr_clients; i++) {
        if (!strcmp(clients[i].id, id)) {
            return i;
        }
    }
    return -1;
}

struct tcp_client* get_client(struct tcp_client* clients, int sockfd, int nr_clients) {
    for (int i = 0; i < nr_clients; i++) {
        if (clients[i].sockfd == sockfd) {
            return &clients[i];
        }
    }
    return NULL;
}

int add_topic(struct tcp_client* client, char* topic, int sf) {
    int n = client->nr_topics;
    // check if already subscribed to topic to prevent duplicates
    for (int i = 0; i < n; i++) {
        if (!strcmp(client->topics[i].name, topic)) {
            client->topics[i].sf = sf; // update sf
            return 0;
        }
    }
    // if not already present, add topic
    n++;
    client->topics = realloc(client->topics, n * sizeof(struct topic));
    client->topics[n - 1].name = strdup(topic);
    client->topics[n - 1].sf = sf;
    client->nr_topics = n;
    return 1;
}

int rm_topic(struct tcp_client* client, char* topic) {
    int n = client->nr_topics;
    for (int i = 0; i < n; i++) {
        if (!strcmp(client->topics[i].name, topic)) {
            free(client->topics[i].name);
            memcpy(&client->topics[i], &client->topics[i + 1], (n - i - 1) * sizeof(struct topic));
            client->topics = realloc(client->topics, (n - 1) * sizeof(struct topic));
            client->nr_topics--;
            return 1;
        }
    }
    return 0;
}

void add_message(struct message* message, struct tcp_client* client) {
    int n = client->nr_messages;
    n++;
    client->messages = realloc(client->messages, n * sizeof(struct message));
    memcpy(&client->messages[n - 1], message, sizeof(struct message));
    client->nr_messages = n;
}

void send_queued_messages(struct tcp_client* client) {
    int nr = client->nr_messages;
    if (nr <= 0) return;
    int i;
    for (i = 0; i < nr; i++) {
        if (!client->sockfd) break;
        tcp_send(client->sockfd, &client->messages[i], sizeof(struct message));
    }
    // rare case: if connection broke while sending queued messages
    if (i < nr - 1) {
        memcpy(&client->messages, &client->messages[i],
                (nr - i -1) * sizeof(struct message));
        client->messages = realloc(client->messages,
                            (nr - i - 1) * sizeof(struct message));
        client->nr_messages = nr - i;
    } else {
        free(client->messages);
        client->nr_messages = 0;
    }
}

void send_to_tcp(struct message* message, struct tcp_client* clients,
                int nr_clients) {
    // iterate through all clients
    for (int i = 0; i < nr_clients; i++) {
        int nr_topics = clients[i].nr_topics;
        struct topic* topics = clients[i].topics;
        // look to whom to send (subscribers only)
        for (int j = 0; j < nr_topics; j++) {
            if (!strcmp(message->topic, topics[j].name)) {
                // check if client connected
                if (clients[i].sockfd) {
                    tcp_send(clients[i].sockfd, message, sizeof(struct message));
                } else {
                    if (!topics[j].sf) continue;
                    add_message(message, &clients[i]); // add to queue if sf ==1
                }
            }
        }
    }
}

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    if (argc < 2) usage(argv[0]);
    fd_set read_fds; //fd set
	fd_set tmp_fds;
	int fdmax; // max val from fd set

	// reset fd sets
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    int udp_socket;
    struct sockaddr_in servaddr;

    // sockfd for udp
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // init server information
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(atoi(argv[1]));

    // bind the socket with the server address
    int rc = bind(udp_socket, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    DIE(rc < 0, "bind failed");

    // add udp socket to fd set
	FD_SET(udp_socket, &read_fds);
	fdmax = udp_socket;

    char buffer[sizeof(struct message)];
    struct sockaddr_in cliaddr;
    socklen_t clilen;

    // TCP
	int tcp_listen_socket, newsockfd;
	int n, i;
	struct tcp_client *clients = NULL;
    int nr_clients = 0;
    int one = 1;

	tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_listen_socket < 0, "socket");
    // disable Nagle's algorithm
    setsockopt(tcp_listen_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&one,
                sizeof(int));

	n = bind(tcp_listen_socket, (struct sockaddr *) &servaddr,
                sizeof(struct sockaddr));
	DIE(n < 0, "bind");

	n = listen(tcp_listen_socket, __INT_MAX__);
	DIE(n < 0, "listen");

	// add stdin and tcp new connections sockets to fd set
    FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcp_listen_socket, &read_fds);
	fdmax = tcp_listen_socket;

    int break_loop = 0; // used to exit while(1)
	while (1) {
        if (break_loop) break;
		tmp_fds = read_fds; 
		
		n = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(n < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (!FD_ISSET(i, &tmp_fds)) continue;

            // check for stdin input
            if (i == STDIN_FILENO) {
                memset(buffer, 0, MAX_SERVER_MESSAGE_LEN);
                read(STDIN_FILENO, buffer, MAX_SERVER_MESSAGE_LEN);
                buffer[strcspn(buffer, "\n")] = 0;
                if (!strcmp(buffer, "exit")) {
                    // disconnect all clients and close server
                    disconnect_all(clients, nr_clients);
                    break_loop = 1;
                    break;
                }
            }
            else if (i == udp_socket) {
                // if udp message received
                memset(&cliaddr, 0, sizeof(cliaddr));
                clilen = sizeof(cliaddr);
                memset(buffer, 0, sizeof(struct message));
                n = recvfrom(udp_socket, buffer, MAX_UDP_MESSAGE_LEN, 0,
                            (struct sockaddr*)&cliaddr, &clilen);
                DIE(n < 0, "recv");
                struct message* message = (struct message*)buffer;
                // set source ip and port to print these on the client side
                message->source_ip = cliaddr.sin_addr;
                message->source_port = cliaddr.sin_port;
                send_to_tcp(message, clients, nr_clients);

            } else if (i == tcp_listen_socket) {
                // new connection request
                clilen = sizeof(cliaddr);
                newsockfd = accept(tcp_listen_socket,
                                    (struct sockaddr *) &cliaddr, &clilen);
                // disable Nagle's algorithm
                setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY,
                            (char*)&one, sizeof(int));
                DIE(newsockfd < 0, "accept");
                
                memset(buffer, 0, MAX_ID_LEN + 1);
                tcp_recv(newsockfd, buffer);
                DIE(n < 0, "recv");

                struct tcp_client *new_client = init_client(buffer);

                if (!add_client(&clients, &nr_clients, new_client, newsockfd)) {
                    // reject client
                    printf("Client %s already connected.\n", new_client->id);
                    disconnect_client(new_client, &clients, &nr_clients, 1);
                    free(new_client);
                    continue;
                }
                printf("New client %s connected from %s:%u.\n",
                        buffer, inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                send_queued_messages(get_client(clients, newsockfd,
                                                nr_clients));

                // add new socket to fd set
                FD_SET(newsockfd, &read_fds);
                if (newsockfd > fdmax) { 
                    fdmax = newsockfd;
                }

            } else {
                // received data from one of connected clients
                memset(buffer, 0, CLIENT_MAX_MSG_LEN);
                tcp_recv(i, buffer);

                if (!strcmp(buffer, "exit")) {
                    // disconnect client
                    struct tcp_client* client = get_client(clients, i, nr_clients);
                    char* tmp_id = strdup(client->id);
                    disconnect_client(client, &clients, &nr_clients, 0);
                    printf("Client %s disconnected.\n", tmp_id);
                    free(tmp_id);
                    
                    // delete socket from fd set
                    FD_CLR(i, &read_fds);
                } else if (!strncmp(buffer, "subscribe", 9)) {
                    // received subscribe command
                    // extract info
                    char topic[50];
                    uint8_t sf = 2;
                    memset(topic, 0, MAX_TOPIC_LEN);
                    sscanf(buffer, "%*s %s %hhu", topic, &sf);
                    if (sf > 1 || buffer[0] == 0) continue;;
                    struct tcp_client* client = get_client(clients, i,
                                                            nr_clients);
                    // save subscription to server memory
                    if (add_topic(client, topic, sf)) {
                        strcpy(buffer, "ssuccess");
                        tcp_send(i, buffer, MAX_SERVER_MESSAGE_LEN);
                    }
                } else if (!strncmp(buffer, "unsubscribe", 11)) {
                    // received unsubscribe command
                    char topic[50];
                    memset(topic, 0, MAX_TOPIC_LEN);
                    sscanf(buffer, "%*s %s", topic);
                    if (buffer[0] == 0) continue;
                    struct tcp_client* client = get_client(clients, i, nr_clients);
                    if (rm_topic(client, topic)) {
                        strcpy(buffer, "usuccess");
                        tcp_send(i, buffer, MAX_SERVER_MESSAGE_LEN);
                    }
                }
            }
		}
	}

    // close other sockets
	close(udp_socket);
    close(tcp_listen_socket);

	return 0;
}