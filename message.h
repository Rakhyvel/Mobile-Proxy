#ifndef MESSAGE_H
#define MESSAGE_H

/*
what's sent before each internal message
cast address to a character array to send through socket
*/
typedef struct {
    char type;
    int length;
    int session_id;
    int msg_num;
} Header;

typedef enum {
    DATA,
    ACK,
    HEARTBEAT,
    END
} MessageType;

void send_raw(int sock, char* data, int num_bytes);
int recv_raw(int sock, char* data, int num_bytes);
Header recv_header(int sock, char** data);

#endif