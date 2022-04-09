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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#endif

#define MAX(x, y) (x > y ? x : y)

// returns 0 if good, 1 if sproxy is closed, -1 if telnet is closed
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

void cproxy(int port, char* ipText , char* portText) {
    struct sockaddr_in sproxyAddr; // address of sproxy
    struct sockaddr_in cproxyAddr; // self address
    struct sockaddr_in telnetAddr; // address for telnet connection    

    // Create telnet socket
    int telnetSock = socket(PF_INET, SOCK_STREAM, 0);
    cproxyAddr.sin_family = AF_INET;
    cproxyAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    cproxyAddr.sin_port = htons(port); // added local to hold spot 
    if (telnetSock < 0) {
        fprintf(stderr,"Unable to create socket");
        exit(1);
    }
    if (bind(telnetSock, (struct sockaddr*)&cproxyAddr, sizeof(cproxyAddr)) < 0) {
        fprintf(stderr,"Unable to Bind");
        exit(1);
    }
    if (listen(telnetSock, 5) < 0) {
        fprintf(stderr,"Unable to Listen");
        exit(1);
    }

    // Accept new telnet connection
    socklen_t telnetLen;
    telnetLen  = sizeof(telnetAddr);
    int telnetCon = accept(telnetSock, (struct sockaddr*)&telnetAddr, &telnetLen);

    bool telnetRunning = true;
    while (telnetRunning) {
        // Connect to sproxy
        int session_id = 1234;
        int sproxySock = socket(PF_INET, SOCK_STREAM, 0);
        if (sproxySock == -1) {
            perror("client failed creating socket");
            exit(1);
        }
        sproxyAddr.sin_family = AF_INET;
        sproxyAddr.sin_port = htons(atoi(portText));
        inet_pton(AF_INET, ipText, &sproxyAddr.sin_addr);
        if (connect(sproxySock, (struct sockaddr*)&sproxyAddr, sizeof(sproxyAddr)) == -1) {
            perror("client failed connecting socket");
            exit(1);
        }

        // sproxy send loop
        int sproxy_connection_status;
        while (!(sproxy_connection_status = is_closed(telnetCon, sproxySock, session_id)));
        if (sproxy_connection_status == -1) {
            telnetRunning = false;
        }

        close(sproxySock);
    }
    close(telnetCon);
    close(telnetSock);
}

int main(int argc, char* argv[]) {
#ifdef CLIENT
//    printf("I am a client\n");
    if (argc < 3) {
        fprintf(stderr, "Usage: client <ip#> <port#>");
    } else {
        cproxy(atoi(argv[1]),argv[2],argv[3]);
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
