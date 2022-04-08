#ifndef MESSAGE_H
#define MESSAGE_H

/*
what's sent before each internal message
cast address to a character array to send through socket
*/
typedef struct {
    char type;
    int length;
} Header;

typedef enum {
    HEARTBEAT,
    DATA,
    END
} MessageType;

void send_raw(int sock, char* data, int num_bytes);
void send_header(int sock, char* data, int num_bytes, MessageType type);
int recv_raw(int sock, char* data, int num_bytes);
Header recv_header(int sock, char** data);

#endif