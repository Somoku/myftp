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
    datagram message = {
        .m_type = 0xA2,
        .m_status = 1,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, (uint8_t*)&message + reply_ret, len - reply_ret, 0);
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
    // Configure AUTH_REPLY to client.
    datagram message = {
        .m_type = 0xA4,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);
    
    if(!strcmp(payload, "user 123123"))
        message.m_status = 1;
    else
        message.m_status = 0;

    // Send server reply.
    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, (uint8_t*)&message + reply_ret, len - reply_ret, 0);
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
    datagram header = {
        .m_status = 0,
        .m_type = 0xA6
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);

    // Acquire ls results.
    FILE* pf = popen("ls", "r");
    if(!pf) {
        fprintf(stderr, "Error: popen error.\n");
        return false;
    }
    char buf[LS_MAX] = {};
    memset(buf, 0, LS_MAX);
    size_t ls_len = fread(buf, 1, LS_MAX - 1, pf);
    header.m_length = htonl(12 + ls_len + 1);
    pclose(pf);

    datagram* message = (datagram*)malloc(ntohl(header.m_length));
    *message = header;
    memcpy(message->payload, buf, ls_len + 1);

    // Send list results to client.
    size_t reply_ret = 0, len = ntohl(message->m_length);
    while (reply_ret < len){
        ssize_t b = send(client, (uint8_t*)message + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(message);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    free(message);
    return true;
}

bool server_get(int client, char* file_name){
    if(!strlen(file_name)){
        fprintf(stderr, "Error: Empty file name.\n");
        return false;
    }

    // Configure message GET_REPLY to client.
    datagram header = {
        .m_type = 0xA8,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);

    // Check whether the file is in local space.
    if(access(file_name, F_OK) != 0){
        header.m_status = 0;

        // Send reply to client.
        size_t reply_ret = 0, len = ntohl(header.m_length);
        while (reply_ret < len){
            ssize_t b = send(client, (uint8_t*)&header + reply_ret, len - reply_ret, 0);
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
        header.m_status = 1;

        // Send reply and file data to client.
        size_t reply_ret = 0, len = ntohl(header.m_length);
        while (reply_ret < len){
            ssize_t b = send(client, (uint8_t*)&header + reply_ret, len - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }

        FILE* down_file = fopen(file_name, "r");
        if(!down_file){
            fprintf(stderr, "Error: Failed to open a file.\n");
            return false;
        }
        fseek(down_file, 0, SEEK_END);
        size_t file_len = ftell(down_file);
        fseek(down_file, 0, SEEK_SET);

        datagram* file_data = (datagram*)malloc(sizeof(datagram) + file_len);
        file_data->m_type = 0xFF;
        file_data->m_status = 0;
        file_data->m_length = htonl(HEAD_SIZE + file_len);
        memcpy(file_data->m_protocol, "\xe3myftp", 6);
        size_t l = fread(file_data->payload, file_len, 1, down_file);

        // Send file data.
        reply_ret = 0, len = ntohl(file_data->m_length);
        while (reply_ret < len){
            ssize_t b = send(client, (uint8_t*)file_data + reply_ret, len - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                free(file_data);
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }
        free(file_data);
        fclose(down_file);
        return true;
    }

    return false;
}

bool server_put(int client, char* file_name){
    // Configure message PUT_REPLY to client.
    datagram message = {
        .m_type = 0xAA,
        .m_status = 0,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, (uint8_t*)&message + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Receive client file data.
    datagram* header = (datagram*)malloc(sizeof(datagram));
    reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(client, (uint8_t*)header + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    
    // Handle FILE_DATA from client.
    if(header->m_type != 0xFF){
        free(header);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    reply_ret = 0;
    len = ntohl(header->m_length) - HEAD_SIZE;
    datagram* reply = (datagram*)malloc(ntohl(header->m_length));
    *reply = *header;
    free(header);
    while(reply_ret < len){
        ssize_t b = recv(client, (uint8_t*)reply->payload + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

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
    datagram message = {
        .m_type = 0xAC,
        .m_status = 1,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send reply to client.
    size_t reply_ret = 0, len = ntohl(message.m_length);
    while (reply_ret < len){
        ssize_t b = send(client, (uint8_t*)&message + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }
    printf("Quit...\n");
    return true;
}