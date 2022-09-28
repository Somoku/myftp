#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

//TODO: Comfirm all len and sizeof(datagram).

enum Command cmd_type(char* cmd){
    char* all_cmds[CMD_NUM] = {"open", "auth", "ls", "get", "put", "quit"};
    for(int i=0;i<CMD_NUM;++i)
        if(!strncmp(cmd, all_cmds[i], strlen(all_cmds[i])))
            return i;
    return INVALID;
}

bool client_open(int sock, char* buf){
    if(state != IDLE){
        fprintf(stderr, "Error: Connection already built.\n");
        return false;
    }

    char* ip_str, *port_str, *token;
    uint16_t port;
    struct sockaddr_in addr;

    // Acquire IP and Port from buf.
    token = strtok(buf, " \n");
    ip_str = strtok(NULL, " \n");
    if(!ip_str){
        fprintf(stderr, "Error: Empty ip.\n");
        return false;
    }
    port_str = strtok(NULL, " \n");
    if(!port_str){
        fprintf(stderr, "Error: Empty port.\n");
        return false;
    }
    port = atoi(port_str);
    if(port == 0 && strcmp(port_str, "0") != 0){
        fprintf(stderr, "Error: Invalid port.\n");
        return false;
    }
    if(strtok(NULL, " \n") != NULL){
        fprintf(stderr, "Error: Invalid command.\n");
        return false;
    }

    // Configure socket.
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip_str, &addr.sin_addr);

    // Build a TCP connection to server.
    if(connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1){
        fprintf(stderr, "Error: Connection failed.\n");
        return false;
    }

    // Configure message OPEN_CONN_REQUEST to server.
    Header message = {
        .m_type = 0xA1,
        .m_status = 0,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, ntohl(message.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(message.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle OPEN_CONN_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xA2){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    if(reply->m_status == 1){
        free(reply);
        state = CONN;
        printf("Successfully connected.\n");
        return true;
    }
    free(reply);
    return false;
}

bool client_auth(int sock, char* buf){
    if(state != CONN){
        fprintf(stderr, "Error: No connection yet.\n");
        return false;
    }

    char* user, *pass, *token;
    
    // Acquire user and pass.
    token = strtok(buf, " \n");
    user = strtok(NULL, " \n");
    if(!user){
        fprintf(stderr, "Error: Empty user.\n");
        return false;
    }
    pass = strtok(NULL, " \n");
    if(!pass){
        fprintf(stderr, "Error: Empty pass.\n");
        return false;
    }
    if(strtok(NULL, " \n") != NULL){
        fprintf(stderr, "Error: Invalid command.\n");
        return false;
    }

    // Configure message AUTH_REQUEST to server.
    datagram message = {
        .header.m_type = 0xA3,
        .header.m_status = 0
    };
    memcpy(message.header.m_protocol, "\xe3myftp", 6);
    memset(message.payload, 0, sizeof(message.payload));
    sprintf(message.payload, "%s %s", user, pass);
    message.header.m_length = htonl(12 + strlen(message.payload) + 1);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, ntohl(message.header.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(message.header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle AUTH_REPLY from server.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    memcpy(reply, buffer, sizeof(datagram));
    if(reply->header.m_type != 0xA4){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
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
    free(reply);
    return false;
}

bool client_ls(int sock){
    if(state != MAIN){
        fprintf(stderr, "Error: No authentication yet.\n");
        return false;
    }

    // Configure message LIST_REQUEST to server.
    Header header = {
        .m_type = 0xA5,
        .m_status = 0,
        .m_length = htonl(12)
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &header, ntohl(header.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle LIST_REPLY from server.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    memcpy(reply, buffer, sizeof(datagram));
    if(reply->header.m_type != 0xA6){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    printf("--- file list start ---\n");
    printf("%s\n", reply->payload);
    printf("--- file list end ---\n");
    free(reply);
    return true;
}

bool client_get(int sock, char* buf){
    if(state != MAIN){
        fprintf(stderr, "Error: No authentication yet.\n");
        return false;
    }

    // Acquire file name.
    char* file_name, *token;
    token = strtok(buf, " \n");
    file_name = strtok(NULL, " \n");
    if(!file_name){
        fprintf(stderr, "Error: Empty file name.\n");
        return false;
    }
    if(strtok(NULL, " \n") != NULL){
        fprintf(stderr, "Error: Invalid command.\n");
        return false;
    }
    
    // Configure message GET_REQUEST to server.
    datagram message = {
        .header.m_type = 0xA7,
        .header.m_status = 0
    };
    memcpy(message.header.m_protocol, "\xe3myftp", 6);
    memset(message.payload, 0, sizeof(message.payload));
    sprintf(message.payload, "%s", file_name);
    message.header.m_length = htonl(12 + strlen(message.payload) + 1);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, ntohl(message.header.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(message.header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    len = 12;
    while(reply_ret < len){
        size_t b = recv(sock, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle GET_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xA8){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    if(reply->m_status == 0){
        free(reply);
        printf("File %s doesn't exist.\n", file_name);
        return true;
    }
    else{
        free(reply);

        // Receive file data from server.
        reply_ret = 0;
        memset(buffer, 0, BUF_SIZE);

        while(reply_ret < BUF_SIZE){
            size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }

        // Handle FILE_DATA from server.
        datagram* reply = (datagram*)malloc(sizeof(datagram));
        memcpy(reply, buffer, sizeof(datagram));
        if(reply->header.m_type != 0xFF){
            free(reply);
            fprintf(stderr, "Error: Reply type error.\n");
            return false;
        }

        // Write file data to local file.
        size_t file_len = ntohl(reply->header.m_length) - 12;
        FILE* down_file = fopen(file_name, "w+");
        if(!down_file){
            free(reply);
            fprintf(stderr, "Error: Failed to open a file.\n");
            return false;
        }
        fwrite(reply->payload, file_len, 1, down_file);
        fclose(down_file);
        
        free(reply);
        return true;
    }
    return false;
}

bool client_put(int sock, char* buf){
    if(state != MAIN){
        fprintf(stderr, "Error: No authentication yet.\n");
        return false;
    }

    // Acquire file name.
    char* file_name, *token;
    char buffer[BUF_SIZE] = {};
    token = strtok(buf, " \n");
    file_name = strtok(NULL, " \n");
    if(!file_name){
        fprintf(stderr, "Error: Empty file name.\n");
        return false;
    }
    if(strtok(NULL, " \n") != NULL){
        fprintf(stderr, "Error: Invalid command.\n");
        return false;
    }

    // Check whether the file is in local space.
    if(access(file_name, F_OK) != 0){
        fprintf(stderr, "Error: File %s doesn't exist.\n", file_name);
        return false;
    }

    // Configure message PUT_REQUEST to server.
    datagram message_put = {
        .header.m_type = 0xA9,
        .header.m_status = 0
    };
    memcpy(message_put.header.m_protocol, "\xe3myftp", 6);
    memset(message_put.payload, 0, sizeof(message_put.payload));
    sprintf(message_put.payload, "%s", file_name);
    message_put.header.m_length = htonl(12 + strlen(message_put.payload) + 1);
    memcpy(buffer, &message_put, ntohl(message_put.header.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(message_put.header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle PUT_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xAA){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    free(reply);
    memset(buffer, 0, BUF_SIZE);

    // Configure message FILE_DATA to server.
    datagram message_data = {
        .header.m_type = 0xFF,
        .header.m_status = 0
    };
    memcpy(message_data.header.m_protocol, "\xe3myftp", 6);
    memset(message_data.payload, 0, sizeof(message_data.payload));
    size_t file_len;
    FILE* up_file = fopen(file_name, "r");
    if(!up_file){
        fprintf(stderr, "Error: Failed to open a file.\n");
        return false;
    }
    fseek(up_file, 0, SEEK_END);
    file_len = ftell(up_file);
    fseek(up_file, 0, SEEK_SET);
    fread(message_data.payload, file_len, 1, up_file);
    message_data.header.m_length = htonl(12 + file_len);
    memcpy(buffer, &message_data, ntohl(message_data.header.m_length));

    // Send file data.
    request_ret = 0, len = ntohl(message_data.header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    fclose(up_file);
    return true;
}

bool client_quit(int sock){
    if(state != MAIN){
        fprintf(stderr, "Error: No authentication yet.\n");
        return false;
    }

    // Configure message QUIT_REQUEST to server.
    Header header = {
        .m_type = 0xAB,
        .m_status = 0,
        .m_length = htonl(12)
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &header, ntohl(header.m_length));

    // Send client request.
    size_t request_ret = 0, len = ntohl(header.m_length);
    while (request_ret < len){
        size_t b = send(sock, buffer + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive server reply.
    size_t reply_ret = 0;
    while(reply_ret < BUF_SIZE){
        size_t b = recv(sock, buffer + reply_ret, BUF_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle LIST_REPLY from server.
    Header* reply = (Header*)malloc(sizeof(Header));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->m_type != 0xAC){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    free(reply);
    close(sock);
    state = EXIT;
    printf("Connection closed.\n");
    return true;
}