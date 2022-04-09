#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "message.h"
#include "queue.h"
#ifdef _WIN64
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif

time_t proxytime = 0;
int heart_beat_count_fails = 0;
int crrent_id = 0;

void start_time(){
    time_t sec;
    sec = time(NULL);
    proxytime = sec;
}

int time_from_heart(){
    time_t sec_c;
    sec_c = time(NULL);
    return sec_c - proxytime;
}
int send_heart_beat(Header *header,int sock){
    send_header(sock, NULL, 0, HEARTBEAT);  
}
int test_heart_beat(Header *header,int sock){
    if(heart_beat_count_fails > 3){
        return -1;
    }
    if(header->type == DATA || header->type == HEARTBEAT){
        heart_beat_count_fails = 0;
    }else if(time_from_heart() > 1){
        send_heart_beat(header,sock);
        heart_beat_count_fails++;
        start_time();
    }
}
int is_closed(int telnetCon, int sproxySock, int session_id) {
    char buff[1024];
    int MAX_LEN = 1024;

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 500000;

    fd_set readfds;
    FD_SET(telnetCon, &readfds);
    FD_SET(sproxySock, &readfds);

    int n = MAX(telnetCon, sproxySock) + 1;
    
    int rv = select(n, &readfds, NULL, NULL, &tv);
    if(rv < 0){
        fprintf(stderr, "Error in select");
        exit(1);
    }

    // if input from telnet, send to sproxy
    if (FD_ISSET(telnetCon, &readfds)) {
        int rev = recv(telnetCon, buff, MAX_LEN, 0);
        if (rev <= 0) {
            return -1;
        }
        push_msg(DATA, session_id, buff, rev);
    }
    // if input from sproxy, send to telnet
    if (FD_ISSET(sproxySock, &readfds)) {
        char* buff2;
        Header header = recv_header(sproxySock, &buff2);
        switch(header.type) {
        case DATA:
            send_raw(telnetCon, buff2, header.length);
            free(buff2);
            push_msg(ACK, session_id, NULL, 0);
            break;
        case ACK:
            pop_front();
            break;
        case HEARTBEAT:
            push_msg(ACK, session_id, NULL, 0);
            break;
        case END:
            return 1;
        }
        // TODO: reset unack counter
    }
    send_front();
    return 0;
}
void sproxy(int port) {
    char buff[1024];
    char buff2[1024];
    int MAX_LEN = 1024;
    int acc, b, l, sock;
    int telnet = 0;
    struct sockaddr_in serverAddr, clientAddr;
    

    char ipText[] = "127.0.0.1";
    char portText[] = "23";
    struct sockaddr_in serverAddr2; // address to connect to
    int sockDeamon; // socket to send to
    // Create socket 'sock'
    sockDeamon = socket(PF_INET, SOCK_STREAM, 0);

    if (sockDeamon == -1) {
        // err
        perror("client failed creating socket");
        exit(1);
    }
    // Connect socket
    serverAddr2.sin_family = AF_INET;
    serverAddr2.sin_port = htons(atoi(portText));
    inet_pton(AF_INET, ipText, &serverAddr2.sin_addr);
    if(connect(sockDeamon, (struct sockaddr*)&serverAddr2, sizeof(serverAddr2)) == -1) {
        perror("client failed connecting socket");
        exit(1);
    }else{
        telnet = 1;
    }

    while(telnet){
    int ID = 1234;
    ////////////////////////////////////////////////
    sock = socket(PF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    serverAddr.sin_port = htons(port);// added local to hold spot 
    
    if(sock < 0){
      fprintf(stderr,"Unable to create socket");
      exit(1);
    }

    b = bind(sock,(struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if(b < 0){
	fprintf(stderr,"Unable to Bind\n");
        exit(1);
    }

    l = listen(sock,5);// pending connections on socket
    if(l < 0){
        fprintf(stderr,"Unable to Listen\n");
        exit(1);
    }

    //accept the conection
    socklen_t client_len;
    client_len  = sizeof(clientAddr);
    acc = accept(sock,(struct sockaddr *)&clientAddr,&client_len);
        if(acc < 0){
           fprintf(stderr,"Unable to accept connection");
           exit(1);
        }
    int cproxy_connection_status;
    while(!(cproxy_connection_status = is_closed(sockDeamon,acc,ID))){
        if(cproxy_connection_status == -1){
            telnet = 0;
        }

    }
    close(acc);
    }
/*
 * Takes in a socket, and a pointer to a buffer, and the length of the data and ensures that
 * the entire chunk of data is received
 * 
 * returns true when the connection is terminated
 */
static bool recv_data(int sock, char* data, int num_bytes) {
    int bytes_recv;
    do {
        bytes_recv = recv(sock, data, num_bytes, 0);
        if (bytes_recv <= 0) {
            return true;
        } else {
            data += bytes_recv;
            num_bytes -= bytes_recv;
        }
    } while(num_bytes > 0);
    return false;
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
            perror("client failed recv data:");
            exit(1);
        } else {
            data += bytes_sent;
            num_bytes -= bytes_sent;
        }
    } while(num_bytes > 0);
}

/*
*Creats and returns socket
*/
int build_socket(){
    int sock;
    sock = socket (PF_INET, SOCK_STREAM,0); // using stream for TCP
    if(sock < 0){
        fprintf(stderr,"Unable to create socket");
        exit(1);
    }
    return sock;
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
    int end = 1;
    // Create socket 'sock'
    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        // err
        perror("client failed creating socket");
        exit(1);
    }

    // Connect socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(portText));
    inet_pton(AF_INET, ipText, &serverAddr.sin_addr);
    if(connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("client failed connecting socket");
        exit(1);
    }
    while(end){ 
     // Pop character from input stream until EOF 
    for (;(c = fgetc(in)) != EOF && c != '\n' && buffer_pos < 1024; buffer_pos++) {
        buffer[buffer_pos] = c;
    }
    
    if(c == EOF){
      break;
    }
    net_buffer_pos = htonl(buffer_pos);
    // Send size and data
    send_data(sock, (char*)&net_buffer_pos, sizeof(net_buffer_pos));
    send_data(sock, buffer, buffer_pos);
    buffer_pos = 0;
    }
    close(sock);
}

int main(int argc, char* argv[]) {
#ifdef CLIENT
//    printf("I am a client\n");
    if (argc < 3) {
        fprintf(stderr, "Usage: client <ip#> <port#>");
    } else {
        char* ip = argv[1];
        char* port = argv[2];
        client(stdin, ip, port);
    }
#else
  //  printf("I am a server\n");
    if(argc == 2){
       server(atoi(argv[1]));
    }else{
       fprintf(stderr,"Error missing sport arg");
    }
#endif
    return 0;
}
