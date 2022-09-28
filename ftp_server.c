#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"
#include "ftp_reply.h"

int main(int argc, char ** argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        fprintf(stderr, "Error: Failed to create a socket.\n");
        return -1;
    }
    struct sockaddr_in addr;
    int client = -1;
    char buf[BUF_SIZE];

    addr.sin_port = htons((uint16_t)atoi(argv[2]));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    if(bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        fprintf(stderr, "Error: Failed to bind socket.\n");
        return -1;
    }
    listen(sock, 128);

    while(true){
        if(client == -1){
            // printf("Waiting for connection...\n");
            client = accept(sock, NULL, NULL);
            if(client == -1){
                fprintf(stderr, "Error: Failed to accept connection.\n");
                return -1;
            }
            // printf("Connection accepted.\n");
        }
        memset(buf, 0, BUF_SIZE);

        // Receive client request.
        size_t request_ret = 0;
        while(request_ret < 12){
            ssize_t b = recv(client, buf + request_ret, 12 - request_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            request_ret += b;
            // printf("request_ret = %u\n", request_ret);
        }

        // Handle client request.
        datagram* request = (datagram*)malloc(sizeof(datagram));
        memcpy(request, buf, sizeof(Header));
        request_ret = 0;
        size_t len = ntohl(request->header.m_length) - 12;
        memset(buf, 0, BUF_SIZE);
        while(request_ret < len){
            ssize_t b = recv(client, buf + request_ret, len - request_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            request_ret += b;
        }
        memcpy(request->payload, buf, len);
        char* payload = request->payload;

        switch(request->header.m_type){
            case 0xA1:
                if(!server_open(client))
                    fprintf(stderr, "Error: Failed to handle open request.\n");
                break;
            case 0xA3:
                if(!server_auth(client, payload)){
                    printf("Client can't pass authentication.\n");
                    close(client);
                    client = -1;
                }
                break;
            case 0xA5:
                if(!server_ls(client))
                    fprintf(stderr, "Error: Failed to handle ls request.\n");
                break;
            case 0xA7:
                if(!server_get(client, payload))
                    fprintf(stderr, "Error: Failed to handle get request.\n");
                break;
            case 0xA9:
                if(!server_put(client, payload))
                    fprintf(stderr, "Error: Failed to handle put request.\n");
                break;
            case 0xAB:
                if(!server_quit(client))
                    fprintf(stderr, "Error: Failed to handle quit request.\n");
                else{
                    close(client);
                    client = -1;
                }
                break;
            default:
                fprintf(stderr, "Error: Invalid request.\n");
        }

        free(request);
    }

    close(sock);
    return 0;
}