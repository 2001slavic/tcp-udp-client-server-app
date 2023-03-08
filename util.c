#include "util.h"

void tcp_send(int sockfd, const void* buffer, size_t buflen) {
    int n;
    n = send(sockfd, &buflen, sizeof(size_t), 0);
    DIE(n < 0, "send");

    n = send(sockfd, buffer, buflen, 0);
    DIE(n < 0, "send");
}

void tcp_recv(int sockfd, void* buffer) {
    int n;
    size_t size;
    size_t received = 0;

    while (received != sizeof(size_t)) {
        n = recv(sockfd, &size + received, sizeof(size_t) - received, 0);
        DIE(n < 0, "recv");
        received += n;
    }
    
    received = 0;
    while (received != size) {
        n = recv(sockfd, buffer + received, size - received, 0);
        DIE(n < 0, "recv");
        received += n;
    }
}
