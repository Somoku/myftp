#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define BUF_SIZE 128
#define CMD_NUM 6

enum Command{
    OPEN,
    AUTH,
    LS,
    GET,
    PUT,
    QUIT,
    INVALID
};

enum Command cmd_type(char* cmd){
    char* all_cmds[CMD_NUM] = {"open", "auth", "ls", "get", "put", "quit"};
    for(int i=0;i<CMD_NUM;++i)
        if(!strncmp(cmd, all_cmds[i], strlen(all_cmds[i])))
            return i;
    return INVALID;
}

int main(int argc, char ** argv) {
    //TODO: Create a socket and build connection.
    
    while(true){
        char buffer[BUF_SIZE];
        printf("Please input your command:\n");
        fgets(buffer, BUF_SIZE, stdin);
        int cmd = cmd_type(buffer);
        switch(cmd){
            case OPEN:
                ftp_open(buffer); break;
            case AUTH:
                ftp_auth(buffer); break;
            case LS:
                ftp_ls(); break;
            case GET:
                ftp_get(buffer); break;
            case PUT:
                ftp_put(buffer); break;
            case QUIT: 
                ftp_quit(); break;
            default:
                printf("Invalid command.\n"); break;
        }
    }
    return 0;
}