#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

bool server_open(int client){
    // Configure message OPEN_CONN_REPLY to client.
    Header message = {
        .m_type = 0xA2,
        .m_status = 1,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    char buffer[BUF_SIZE] = {};
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &message, ntohl(message.m_length));
    
    // send(client, buffer, ntohl(message.m_length), 0);

    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    return true;
}

bool server_auth(int client, char* payload){
    // printf("user name = %s\n", payload);
    
    // Configure AUTH_REPLY to client.
    Header message = {
        .m_type = 0xA4,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);
    
    if(!strcmp(payload, "user 123123"))
        message.m_status = 1;
    else
        message.m_status = 0;
    
    char buffer[BUF_SIZE] = {};
    memcpy(buffer, &message, ntohl(message.m_length));

    // Send server reply.
    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    if(!message.m_status)
        return false;
    else
        return true;
}

bool server_ls(int client){
    // Configure message LIST_REPLY to client.
    datagram message = {
        .header.m_status = 0,
        .header.m_type = 0xA6
    };
    memcpy(message.header.m_protocol, "\xe3myftp", 6);
    memset(message.payload, 0, MAX_PAYLOAD);

    // Acquire ls results.
    FILE* pf = popen("ls", "r");
    if(!pf) {
        fprintf(stderr, "Error: popen error.\n");
        return false;
    }
    size_t ls_len = fread(message.payload, 1, MAX_PAYLOAD, pf);
    message.header.m_length = htonl(12 + ls_len + 1);
    pclose(pf);

    // printf("%s", message.payload);

    // Send list results to client.
    char buffer[BUF_SIZE] ={};
    memcpy(buffer, &message, ntohl(message.header.m_length));
    size_t reply_ret = 0, len = ntohl(message.header.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    return true;
}

bool server_get(int client, char* file_name){
    if(!strlen(file_name)){
        fprintf(stderr, "Error: Empty file name.\n");
        return false;
    }

    // Configure message GET_REPLY to client.
    Header message = {
        .m_type = 0xA8,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Check whether the file is in local space.
    if(access(file_name, F_OK) != 0){
        message.m_status = 0;

        // Send reply to client.
        char buffer[BUF_SIZE] = {};
        memset(buffer, 0, BUF_SIZE);
        memcpy(buffer, &message, ntohl(message.m_length));
        
        // send(client, buffer, ntohl(message.m_length), 0);

        size_t reply_ret = 0, len = ntohl(message.m_length);
        while (reply_ret < len){
            ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }
        return true;
    }
    else{
        message.m_status = 1;

        // Send reply and file data to client.
        char buffer[BUF_SIZE] = {};
        memset(buffer, 0, BUF_SIZE);
        memcpy(buffer, &message, ntohl(message.m_length));

        datagram file_data = {
            .header.m_type = 0xFF,
            .header.m_status = 0
        };
        memcpy(file_data.header.m_protocol, "\xe3myftp", 6);
        FILE* down_file = fopen(file_name, "r");
        if(!down_file){
            fprintf(stderr, "Error: Failed to open a file.\n");
            return false;
        }

        fseek(down_file, 0, SEEK_END);
        size_t file_len = ftell(down_file);
        fseek(down_file, 0, SEEK_SET);
        
        fread(file_data.payload, file_len, 1, down_file);
        file_data.header.m_length = htonl(12 + file_len);
        memcpy(buffer + ntohl(message.m_length), &file_data, ntohl(file_data.header.m_length));

        // send(client, buffer, ntohl(message.m_length) + ntohl(file_data.header.m_length), 0);

        // Send file data.
        size_t reply_ret = 0, len = ntohl(message.m_length) + ntohl(file_data.header.m_length);
        while (reply_ret < len){
            ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }
        fclose(down_file);
        return true;
    }

    return false;
}

bool server_put(int client, char* file_name){
    // Configure message PUT_REPLY to client.
    Header message = {
        .m_type = 0xAA,
        .m_status = 0,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    char buffer[BUF_SIZE] = {};
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &message, ntohl(message.m_length));
    
    //send(client, buffer, ntohl(message.m_length), 0);

    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    memset(buffer, 0, BUF_SIZE);

    // Receive client file data.
    reply_ret = 0;
    while(reply_ret < 12){
        ssize_t b = recv(client, buffer + reply_ret, 12 - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    
    // Handle FILE_DATA from client.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    memcpy(reply, buffer, sizeof(Header));
    if(reply->header.m_type != 0xFF){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    reply_ret = 0;
    memset(buffer, 0, BUF_SIZE);
    len = ntohl(reply->header.m_length) - 12;
    while(reply_ret < len){
        ssize_t b = recv(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    memcpy(reply->payload, buffer, len);

    // Write to local file.
    size_t file_len = len;
    FILE* up_file = fopen(file_name, "w+");
    if(!up_file){
        free(reply);
        fprintf(stderr, "Error: Failed to open a file.\n");
        return false;
    }
    fwrite(reply->payload, file_len, 1, up_file);
    fclose(up_file);
    free(reply);

    return true;
}

bool server_quit(int client){
    // Configure message QUIT_REPLY to client.
    Header message = {
        .m_type = 0xAC,
        .m_status = 1,
        .m_length = htonl(12)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    char buffer[BUF_SIZE] = {};
    memset(buffer, 0, BUF_SIZE);
    memcpy(buffer, &message, ntohl(message.m_length));
    
    // send(client, buffer, ntohl(message.m_length), 0);

    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, buffer + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    return true;
}