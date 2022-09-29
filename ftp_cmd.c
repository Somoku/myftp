#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

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
    datagram message = {
        .m_type = 0xA1,
        .m_status = 0,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send client request.
    size_t request_ret = 0, len = ntohl(message.m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)&message + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    // Receive server reply.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)reply + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(reply);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle OPEN_CONN_REPLY from server.
    if(reply->m_type != 0xA2){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    if(reply->m_status == 1){
        free(reply);
        state = CONN;
        printf("Server connection accepted.\n");
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
    datagram header = {
        .m_type = 0xA3,
        .m_status = 0
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    header.m_length = htonl(HEAD_SIZE + strlen(user) + 1 + strlen(pass) + 1);
    
    datagram* message = (datagram*)malloc(ntohl(header.m_length));
    *message = header;
    sprintf(message->payload, "%s %s", user, pass);

    // Send client request.
    size_t request_ret = 0, len = ntohl(message->m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)message + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(message);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    free(message);

    // Receive server reply.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)reply + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(reply);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle AUTH_REPLY from server.
    if(reply->m_type != 0xA4){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    if(reply->m_status == 1){
        free(reply);
        state = MAIN;
        printf("Authentication granted.\n");
        return true;
    }
    else{
        free(reply);
        state = IDLE;
        printf("Error: Authentication rejected. Connection closed.\n");
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
    datagram message = {
        .m_type = 0xA5,
        .m_status = 0,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(message.m_protocol, "\xe3myftp", 6);

    // Send client request.
    size_t request_ret = 0, len = ntohl(message.m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)&message + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    // Receive server reply.
    datagram* header = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)header + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(header);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    datagram* reply = (datagram*)malloc(ntohl(header->m_length));
    *reply = *header;
    free(header);

    // Handle LIST_REPLY from server.
    if(reply->m_type != 0xA6){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    reply_ret = 0;
    len = ntohl(reply->m_length) - 12;
    while(reply_ret < len){
        ssize_t b = recv(sock, (uint8_t*)reply->payload + reply_ret, len - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(reply);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    printf("----- file list start -----\n");
    printf("%s", reply->payload);
    printf("----- file list end -----\n");
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
    datagram header = {
        .m_type = 0xA7,
        .m_status = 0
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    header.m_length = htonl(HEAD_SIZE + strlen(file_name) + 1);
    datagram* message = (datagram*)malloc(ntohl(header.m_length));
    *message = header;
    memset(message->payload, 0, ntohl(header.m_length) - HEAD_SIZE);
    sprintf(message->payload, "%s", file_name);

    // Send client request.
    size_t request_ret = 0, len = ntohl(message->m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)message + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(message);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    free(message);

    // Receive server reply.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)reply + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(reply);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle GET_REPLY from server.
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
        datagram* header = (datagram*)malloc(sizeof(datagram));
        while(reply_ret < HEAD_SIZE){
            ssize_t b = recv(sock, (uint8_t*)header + reply_ret, HEAD_SIZE - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                free(header);
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }

        // Handle FILE_DATA from server.
        datagram* reply = (datagram*)malloc(ntohl(header->m_length));
        *reply = *header;
        free(header);
        if(reply->m_type != 0xFF){
            free(reply);
            fprintf(stderr, "Error: Reply type error.\n");
            return false;
        }

        len = ntohl(reply->m_length) - HEAD_SIZE;
        reply_ret = 0;
        while(reply_ret < len){
            ssize_t b = recv(sock, (uint8_t*)reply->payload + reply_ret, len - reply_ret, 0);
            if(b == 0) break;
            else if(b < 0){
                fprintf(stderr, "Error: ?\n");
                return false;
            }
            reply_ret += b;
        }

        // Write file data to local file.
        size_t file_len = len;
        FILE* down_file = fopen(file_name, "w+");
        if(!down_file){
            free(reply);
            fprintf(stderr, "Error: Failed to open a file.\n");
            return false;
        }
        fwrite(reply->payload, file_len, 1, down_file);
        fclose(down_file);
        
        free(reply);
        printf("File downloaded.\n");
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
    datagram header = {
        .m_type = 0xA9,
        .m_status = 0
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);
    header.m_length = htonl(HEAD_SIZE + strlen(file_name) + 1);
    datagram* message = (datagram*)malloc(ntohl(header.m_length));
    *message = header;
    sprintf(message->payload, "%s", file_name);

    // Send client request.
    size_t request_ret = 0, len = ntohl(message->m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)message + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    // Receive server reply.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)reply + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle PUT_REPLY from server.
    if(reply->m_type != 0xAA){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    free(reply);

    // Configure message FILE_DATA to server.
    size_t file_len;
    FILE* up_file = fopen(file_name, "r");
    if(!up_file){
        fprintf(stderr, "Error: Failed to open a file.\n");
        return false;
    }
    fseek(up_file, 0, SEEK_END);
    file_len = ftell(up_file);
    fseek(up_file, 0, SEEK_SET);

    datagram message_head = {
        .m_type = 0xFF,
        .m_status = 0
    };
    memcpy(message_head.m_protocol, "\xe3myftp", 6);
    message_head.m_length = htonl(12 + file_len);
    datagram* file_data = (datagram*)malloc(ntohl(message_head.m_length));
    *file_data = message_head;
    size_t read_byte = fread(file_data->payload, 1, file_len, up_file);
    if(read_byte != file_len){
        fprintf(stderr, "Error: Can't read the entire file.\n");
        return false;
    }

    // Send file data.
    request_ret = 0, len = ntohl(file_data->m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)file_data + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            free(file_data);
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }
    free(file_data);
    fclose(up_file);
    printf("File uploaded.\n");
    return true;
}

bool client_quit(int sock){
    if(state == IDLE){
        printf("Thank you.\n");
        state = EXIT;
        return true;
    }
    else if(state == CONN){
        printf("Thank you.\n");
        state = EXIT;
        return true;
    }

    // Configure message QUIT_REQUEST to server.
    datagram header = {
        .m_type = 0xAB,
        .m_status = 0,
        .m_length = htonl(HEAD_SIZE)
    };
    memcpy(header.m_protocol, "\xe3myftp", 6);

    // Send client request.
    size_t request_ret = 0, len = ntohl(header.m_length);
    while (request_ret < len){
        ssize_t b = send(sock, (uint8_t*)&header + request_ret, len - request_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        request_ret += b;
    }

    // Receive server reply.
    datagram* reply = (datagram*)malloc(sizeof(datagram));
    size_t reply_ret = 0;
    while(reply_ret < HEAD_SIZE){
        ssize_t b = recv(sock, (uint8_t*)reply + reply_ret, HEAD_SIZE - reply_ret, 0);
        if(b == 0) break;
        else if(b < 0){
            fprintf(stderr, "Error: ?\n");
            return false;
        }
        reply_ret += b;
    }

    // Handle LIST_REPLY from server.
    if(reply->m_type != 0xAC){
        free(reply);
        fprintf(stderr, "Error: Reply type error.\n");
        return false;
    }
    free(reply);
    state = EXIT;
    printf("Thank you.\n");
    return true;
}