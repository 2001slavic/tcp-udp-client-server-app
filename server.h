#ifndef _SERVER_H_
#define _SERVER_H_

struct topic {
    char* name;
    int sf;
};

struct tcp_client {
    char* id;
    int sockfd;
    int nr_topics;
    struct topic* topics; // list of subscribed topics
    struct message* messages; // messages queue (array)
    int nr_messages;
};

// prints how server should be started
void usage(char* filename);
// allocate memory and initialize client
struct tcp_client* init_client(char* id);
// adds client to server's memory
int add_client(struct tcp_client** clients, int* nr_clients,
                struct tcp_client* new_client, int sockfd);
// subscribes a client to a topic
int add_topic(struct tcp_client* client, char* topic, int sf);
// removes client from server's memory
void rm_client(struct tcp_client** clients, char* id, int* nr_clients);
// unsubscribes a client from a topic
int rm_topic(struct tcp_client* client, char* topic);
// sends topic update (message) to TCP client
void send_to_tcp(struct message* message, struct tcp_client* clients,
                int nr_clients);
// adds a message to send queue (for SF == 1)
void add_message(struct message* message, struct tcp_client* client);
// sends all queued messages to respective clients
void send_queued_messages(struct tcp_client* client);
// disconnects a client from server
void disconnect_client(struct tcp_client* client, struct tcp_client** clients,
                        int* nr_clients, int refuse);
// disconnects all clients
void disconnect_all(struct tcp_client* clients, int nr_clients);
// returns index of client with given id
int get_client_by_id(struct tcp_client* clients, char* id, int nr_clients);
// get client by socket
struct tcp_client* get_client(struct tcp_client* clients, int sockfd,
                                int nr_clients);

#endif