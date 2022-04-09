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



#define MAX(x, y) (x > y ? x : y)

// returns 0 if good, 1 if sproxy is closed, -1 if telnet is closed
int is_closed(int telnetCon, int proxySock, int session_id) {
    char buff[1024];
    int MAX_LEN = 1024;

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 500000;

    fd_set readfds;
    FD_SET(telnetCon, &readfds);
    FD_SET(proxySock, &readfds);

    int n = MAX(telnetCon, proxySock) + 1;
    
    int rv = select(n, &readfds, NULL, NULL, &tv);
    if (rv < 0){
        fprintf(stderr, "Error in select");
        exit(1);
    }

    // if input from telnet, send to proxy
    if (FD_ISSET(telnetCon, &readfds)) {
        int rev = recv(telnetCon, buff, MAX_LEN, 0);
        if (rev <= 0) {
            return -1;
        }
        push_msg(DATA, session_id, buff, rev);
    }
    // if input from proxy, send to telnet
    if (FD_ISSET(proxySock, &readfds)) {
        char* buff2;
        Header header = recv_header(proxySock, &buff2);
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
        push_msg(HEARTBEAT, session_id, NULL, 0);
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
