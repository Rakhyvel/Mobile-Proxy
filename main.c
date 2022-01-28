#include <stdio.h>
#include <stdlib.h>
//#include <sys/socket.h>

#define CLIENT

/* 
 * Awaits a connection, reads in packet including header and payload, prints out payload
 */
void server() {

}

/*
 * Takes in an input stream, reads each character and adds it to a buffer, then
 * sends the size and buffer to a server connection
 */
void client(FILE* in) {
    unsigned int buffer_size = 255; // size of the allocated mem for buffer
    unsigned int buffer_pos = 0; // cursor into buffer
    char* buffer = calloc(buffer_size, sizeof(char)); // Buffer of chars read from input stream
    if (!buffer) {
        fprintf(stderr, "Error allocating %d bytes for buffer in client() line %d\n", buffer_size, __LINE__);
        exit(1);
    }
    int c; // Char retrieved from input stream, will be EOF at end of file
    // Pop character from input stream until EOF
    while ((c = fgetc(in)) != EOF) {
        if (buffer_pos >= buffer_size) {
            // Resize buffer if too small
            buffer_size *= 2;
            char* newBuffer = realloc(buffer, buffer_size);
            if (!newBuffer) {
                fprintf(stderr, "Error reallocating %d bytes for buffer in client() line %d\n", buffer_size, __LINE__);
                exit(1);
            } else {
                buffer = newBuffer;
            }
        } else {
            // Add char to buffer, increment caret
            buffer[buffer_pos] = c;
            buffer_pos++;
        }
    }
    // Send packet
    free(buffer);
}

int main(int argc, char** argv) {
#ifdef CLIENT
    printf("I am a client\n");
    client(stdin);
#else
    printf("I am a server\n");
    server();
#endif
}