#include <stdint.h>

#define MAGIC_NUMBER_LENGTH 6
#define BUF_SIZE 128
#define MAX_PAYLOAD 2048
#define CMD_NUM 6

typedef unsigned char byte;
typedef unsigned char type;
typedef unsigned char status;

typedef struct{
    byte m_protocol[MAGIC_NUMBER_LENGTH]; /* protocol magic number (6 bytes) */
    type m_type;                          /* type (1 byte) */
    status m_status;                      /* status (1 byte) */
    uint32_t m_length;                    /* length (4 bytes) in Big endian*/
} __attribute__ ((packed)) Header;

typedef struct{
    Header header;
    char payload[MAX_PAYLOAD];
}__attribute__ ((packed)) datagram;

enum Command{
    OPEN,
    AUTH,
    LS,
    GET,
    PUT,
    QUIT,
    INVALID
};

enum State{
    IDLE,
    CONN,
    MAIN,
    EXIT
};

enum State state;

enum Command cmd_type(char*);
bool ftp_open(int, char*);
bool ftp_auth(int, char*);
bool ftp_ls(int);
bool ftp_quit(int);
bool ftp_get(int, char*);
void ftp_put(int, char*);