#include "message.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdbool.h>

/*
Sends data through a socket WITHOUT header

@param sock         socket to send on
@param data         a pointer to a character buffer to where the data is stored
@param num_bytes    how long the buffer is
*/
void send_raw(int sock, char* data, int num_bytes) {
    int bytes_sent;

    do {
        bytes_sent = send(sock, data, num_bytes, 0);
        if (bytes_sent == -1) {
            perror("failed send data");
            exit(1);
        } else {
            data += bytes_sent;
            num_bytes -= bytes_sent;
        }
    } while(num_bytes > 0);
}

/*
Sends data through a socket. Should only be called by queue! Use push_msg if you're a proxy

@param sock         socket to send on
@param data         a pointer to a character buffer to where the data is stored
@param num_bytes    how long the buffer is
*/
void send_header(int sock, char* data, Header header) {
    // Send header
    send_raw(sock, (char*)(&header), sizeof(header));

    // Send data
    if (header.type == DATA) {
        send_raw(sock, data, header.length);
    }
}

/*
Receives raw data from a socket WITHOUT header and puts it into a buffer

@param sock         socket to receive on
@param data         a pointer to a character buffer to place the received data in
@param num_bytes    how long the buffer is
@returns            the number of bytes received from the socket, or -1 if any errors occured during recv
*/
int recv_raw(int sock, char* data, int num_bytes) {
    int bytes_recv;
    int total_bytes = 0;
    bool wellRecv = false; // Initially false, set to true on a good recv

    do {
        bytes_recv = recv(sock, data, num_bytes, 0);
        printf("%d\n",bytes_recv);
        if (bytes_recv <= 0) {
            if (wellRecv) {
                return total_bytes;
            } else {
                return -1;
            }
        } else {
            wellRecv = true;
            total_bytes += bytes_recv;
            data += bytes_recv;
            num_bytes -= bytes_recv;
        }
    } while(num_bytes > 0);
    return total_bytes;
}

/*
Receives data from a socket. Allocates buffer and fills with data, returns header

@param sock         socket to receive on
@param data         a pointer to a pointer to character buffer that will be filled with data.
                    Data is only filled if header type is DATA
@returns            the number of bytes received from the socket
*/
Header recv_header(int sock, char** data) {
    // Receive header
    Header header;

    if (recv_raw(sock, (char*)(&header), sizeof(header)) == -1) {
        // Message is actually a termination
        header.type = END;
        printf("bad header\n");
    } else if (header.type == DATA){
        // Receive data
        *data = calloc(header.length, sizeof(char) + 1);
        if (header.length != 0 && recv_raw(sock, *data, header.length) == -1) {
            // Bad read on data, assume termination
            printf("bad data %d\n", header.length);
            header.type = END;
            free(*data);
        } else {
            printf("Data: %s\n", *data);
        }
    }

    return header;
}