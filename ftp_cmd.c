#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

//TODO: Implement all commands.
//TODO: Change all sizeof(message/header) to m_length.

enum Command cmd_type(char* cmd){
    char* all_cmds[CMD_NUM] = {"open", "auth", "ls", "get", "put", "quit"};
    for(int i=0;i<CMD_NUM;++i)
        if(!strncmp(cmd, all_cmds[i], strlen(all_cmds[i])))
            return i;
    return INVALID;
}

bool ftp_open(int sock, char* buf){
    if(state != IDLE){
        printf("Error: Connection already built.\n");
        return false;
    }

    char* ip_str, port_str, token;
    int port;
    struct sockaddr_in addr;

    // Acquire IP and Port from buf.
    token = strtok(buf, " ");
    ip_str = strtok(NULL, " ");
    if(!ip_str){
        printf("Error: Empty ip.\n");
        return false;
    }
    port_str = strtok(NULL, " ");
    if(!port_str){
        printf("Error: Empty port.\n");
        return false;
    }
    port = atoi(port_str);
    if(port == 0 && strcmp(port, "0") != 0){
        printf("Error: Invalid port.\n");
        return false;
    }
    if(strtok(NULL, " ") != NULL){
        printf("Error: Invalid command.\n");
        return false;
    }

    // Configure socket.
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip_str, &addr.sin_addr);

    // Build a TCP connection to server.
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        printf("Error: Connection failed.\n");
        return false;
    }

    // Configure message OPEN_CONN_REQUEST to server.
    Header message = {
        .m_type = 0xA1,
        .m_status = 0,
        .m_length = 12
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, sizeof(message));
    
    // Send client request.
    size_t request_ret = 0, len = sizeof(message);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    
    // Handle OPEN_CONN_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xA2){
        free(reply);
        printf("Error: Reply type error.\n");
        return false;
    }
    if(reply->m_status == 1){
        free(reply);
        state = CONN;
        return true;
    }
    free(reply);
    return false;
}

bool ftp_auth(int sock, char* buf){
    if(state != CONN){
        printf("Error: No connection yet.\n");
        return false;
    }

    char* user, pass, token;
    
    // Acquire user and pass.
    token = strtok(buf, " ");
    user = strtok(NULL, " ");
    if(!user){
        printf("Error: Empty user.\n");
        return false;
    }
    pass = strtok(NULL, " ");
    if(!pass){
        printf("Error: Empty pass.\n");
        return false;
    }
    if(strtok(NULL, " ") != NULL){
        printf("Error: Invalid command.\n");
        return false;
    }

    // Configure message AUTH_REQUEST to server.
    datagram message = {
        .header.m_type = 0xA3,
        .header.m_status = 0,
        .header.m_length = 12,
    };
    memcpy(message.header.m_protocol, "\xe3myftp", 6);
    memset(message.payload, 0, sizeof(message.payload));
    sprintf(message.payload, "%s %s", user, pass);
    message.header.m_length += (strlen(message.payload) + 1);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, sizeof(message));

    // Send client request.
    size_t request_ret = 0, len = sizeof(message);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle AUTH_REPLY from server.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    memcpy(reply, buffer, sizeof(datagram));
    if(reply->header.m_type != 0xA4){
        free(reply);
        printf("Error: Reply type error.\n");
        return false;
    }
    if(reply->header.m_status == 1){
        free(reply);
        state = MAIN;
        return true;
    }
    else{
        free(reply);
        close(sock);
        state = IDLE;
        printf("Connection closed.\n");
        return false;
    }
    return false;
}

bool ftp_ls(int sock){
    if(state != MAIN){
        printf("Error: No authentication yet.\n");
        return false;
    }

    // Configure message LIST_REQUEST to server.
    Header header = {
        .m_type = 0xA5,
        .m_status = 0,
        .m_length = 12,
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));

    // Send client request.
    size_t request_ret = 0, len = sizeof(header);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle LIST_REPLY from server.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    memcpy(reply, buffer, sizeof(datagram));
    if(reply->header.m_type != 0xA6){
        free(reply);
        printf("Error: Reply type error.\n");
        return false;
    }
    printf("--- file list start ---\n");
    printf("%s\n", reply->payload);
    printf("--- file list end ---\n");
    free(reply);
    return true;
}

void ftp_get(int sock, char* buf){
    if(state != MAIN){
        printf("Error: No authentication yet.\n");
        return false;
    }

}

void ftp_put(int sock, char* buf){
    if(state != MAIN){
        printf("Error: No authentication yet.\n");
        return false;
    }

}

bool ftp_quit(int sock){
    if(state != MAIN){
        printf("Error: No authentication yet.\n");
        return false;
    }

    // Configure message QUIT_REQUEST to server.
    Header header = {
        .m_type = 0xAB,
        .m_status = 0,
        .m_length = 12,
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &header, sizeof(header));

    // Send client request.
    size_t request_ret = 0, len = sizeof(header);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            printf("Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle LIST_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xAC){
        free(reply);
        printf("Error: Reply type error.\n");
        return false;
    }
    free(reply);
    close(sock);
    state = EXIT;
    printf("Connection closed.\n");
    return true;
}