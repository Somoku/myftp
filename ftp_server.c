#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

int main(int argc, char ** argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_port = htons((uint16_t)atoi(argv[2]));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 128);
    int client = accept(sock, NULL, NULL);

    char buf[128];
    size_t l = recv(client, buf, 128, 0);
    
    // send(client, buffer, l, 0);

    // Configure message OPEN_CONN_REQUEST to server.
    Header message = {
        .m_type = 0xA2,
        .m_status = 1,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, ntohl(message.m_length));
    send(client, buffer, ntohl(message.m_length), 0);

    close(client);
    close(sock);

    return 0;
}