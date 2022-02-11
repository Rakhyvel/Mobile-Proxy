#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN64
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#endif

#define CLIENT

/* 
 * Awaits a connection, reads in packet including header and payload, prints out payload
 */
void server() {

}

/*
 * Takes in a socket, some data, and the length of the data and ensures that
 * the entire chunk of data is transfered
 */
static void send_data(int sock, char* data, int num_bytes) {
    int bytes_sent;
    do {
        bytes_sent = send(sock, data, num_bytes, 0);
        if (bytes_sent == -1) {
            perror("client failed sending data");
            exit(1);
        } else {
            data += bytes_sent;
            num_bytes -= bytes_sent;
        }
    } while(num_bytes > 0);
}

/*
 * Takes in an input stream, reads each character and adds it to a buffer, then
 * sends the size and buffer to a server connection
 */
void client(FILE* in, char* ipText, char* portText) {
    int c; // Char retrieved from input stream, will be EOF at end of file
    unsigned int buffer_pos = 0; // cursor into buffer
    int net_buffer_pos; // big endian version of buffer_pos
    char buffer[1024]; // character buffer to store input
    struct sockaddr_in serverAddr; // address to connect to
    int sock; // socket to send to

    // Pop character from input stream until EOF
    for (;(c = fgetc(in)) != EOF && c != '\n' && buffer_pos < 1024; buffer_pos++) {
        buffer[buffer_pos] = c;
    }
    net_buffer_pos = htonl(buffer_pos);

    // Create socket 'sock'
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        // err
        perror("client failed creating socket");
        exit(1);
    }

    // Connect socket
    inet_pton(AF_INET, ipText, &(serverAddr.sin_addr));
    serverAddr.sin_port = atoi(portText);
    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("client failed connecting socket");
        exit(1);
    }

    // Send size and data
    send_data(sock, (char*)&net_buffer_pos, sizeof(net_buffer_pos));
    send_data(sock, buffer, buffer_pos);
}

int main(int argc, char** argv) {
#ifdef CLIENT
    printf("I am a client\n");
    if (argc < 3) {
        fprintf(stderr, "Usage: client <ip#> <port#>");
    } else {
        char* ip = argv[1];
        char* port = argv[2];
        client(stdin, ip, port);
    }
#else
    printf("I am a server\n");
    server();
#endif
    return 0;
}
