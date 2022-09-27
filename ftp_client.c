#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ftp_cmd.h"

int main(int argc, char ** argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1){
        fprintf(stderr, "Error: Socket creation failed.\n");
        return -1;
    }

    // Initial state.
    state = IDLE;
    
    while(true){
        if(state == EXIT)
            break;

        char buffer[BUF_SIZE];
        printf("Please input your command:\n");
        fgets(buffer, BUF_SIZE, stdin);
        
        int cmd = cmd_type(buffer);
        switch(cmd){
            case OPEN:
                if(!ftp_open(sock, buffer))
                    fprintf(stderr, "Error: Failed to open a connection.\n");
                break;
            case AUTH:
                if(!ftp_auth(sock, buffer))
                    fprintf(stderr, "Error: Failed of authentication.\n");
                break;
            case LS:
                if(!ftp_ls(sock))
                    fprintf(stderr, "Error: Failed to ls.\n");
                break;
            case GET:
                ftp_get(sock, buffer);
                break;
            case PUT:
                ftp_put(sock, buffer);
                break;
            case QUIT: 
                if(!ftp_quit(sock))
                    fprintf(stderr, "Error: Failed to quit.\n");
                break;
            default:
                fprintf(stderr, "Error: Invalid command.\n");
        }
    }
    return 0;
}