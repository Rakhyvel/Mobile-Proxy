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
#include <time.h>
#include <errno.h>
#endif

#define MAX(x, y) (x > y ? x : y)

time_t proxytime = 0;
int heart_beat_count_fails = 0;
int crrent_id = 0;

void start_time() {
    time_t sec;
    sec = time(NULL);
    proxytime = sec;
}

int time_from_heart() {
    time_t sec_c;
    sec_c = time(NULL);
    return sec_c - proxytime;
}

void send_heart_beat(Header header, int sock, int session_id) {
    send_header(sock, NULL, header); 
}

int test_heart_beat(Header header, int sock, int session_id) {
    if (heart_beat_count_fails > 3) {
        return -1;
    }
    if (header.type == DATA || header.type == HEARTBEAT) {
        heart_beat_count_fails = 0;
    } else if (time_from_heart() > 1) {
        send_heart_beat(header,sock,session_id);
        heart_beat_count_fails++;
        start_time();
    }
    return 0;
}

// Used to connect to a server as a client
int connect_client(char* ipText, char* portText) {
    struct sockaddr_in addr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("client failed creating socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(portText));
    inet_pton(AF_INET, ipText, &addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("client failed connecting socket");
        exit(1);
    }
    return sock;
}

// Used to accept a connection from a client as a server
int connect_server(short port) {
    struct sockaddr_in thisAddr, addr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    thisAddr.sin_family = AF_INET;
    thisAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    thisAddr.sin_port = htons(port); // added local to hold spot 
    if (sock < 0) {
        fprintf(stderr,"Unable to create socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr*)&thisAddr, sizeof(thisAddr)) < 0) {
        fprintf(stderr,"Unable to Bind");
        exit(1);
    }
    if (listen(sock, 5) < 0) {
        fprintf(stderr,"Unable to Listen");
        exit(1);
    }
    // Accept new telnet connection
    socklen_t addrLen  = sizeof(addr);
    int serverCon = accept(sock, (struct sockaddr*)&addr, &addrLen); // blocks!
    return serverCon;
}

// returns 0 if good, 1 if sproxy is closed, -1 if telnet is closed
int is_closed(int telnet_connection, int proxySock, int session_id) {
    char buff[1024];
    int MAX_LEN = 1024;

    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 500000;

    fd_set readfds;
    FD_SET(telnet_connection, &readfds);
    FD_SET(proxySock, &readfds);

    int n = MAX(telnet_connection, proxySock) + 1;
    
    int rv = select(n, &readfds, NULL, NULL, &tv);
    if (rv < 0){
        fprintf(stderr, "Error in select");
        exit(1);
    }

    // if input from telnet, send to proxy
    if (FD_ISSET(telnet_connection, &readfds)) {
        int rev = recv(telnet_connection, buff, MAX_LEN, 0);
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
            send_raw(telnet_connection, buff2, header.length);
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
        if (test_heart_beat(header,proxySock,session_id)) {
            return 1;
        }
    }
    send_front();
    return 0;
}


void sproxy(int port) {
    // Connect to telnet daemon
    int telnetDeamon_connection = connect_client("127.0.0.1", "23");

    int telnet_running = 0;
    int ID = -1;
    while (telnet_running) {
        int cproxy_connection = connect_server(port);
        Header header = recv_header(cproxy_connection, NULL);
        if (header.session_id != ID) {
            printf("Session has changed!");
            ID = header.session_id;
            reset_queue();
        }

        int cproxy_connection_status;
        while (!(cproxy_connection_status = is_closed(telnetDeamon_connection, cproxy_connection, ID))) {
            if (cproxy_connection_status == -1) {
                telnet_running = 0;
            }
        }
        close(cproxy_connection);
        reset_await_status();
    }
    close(telnetDeamon_connection);
}

void cproxy(int port, char* ipText , char* portText) {
    // Create telnet server socket
    int telnet_connection = connect_server(22);

    bool telnet_running = true;
    while (telnet_running) {
        int session_id = 1234;

        // Connect to sproxy socket
        int sproxy_connection = connect_client(ipText, portText);

        // sproxy send loop
        push_msg(HEARTBEAT, session_id, NULL, 0);
        int sproxy_connection_status;
        while (!(sproxy_connection_status = is_closed(telnet_connection, sproxy_connection, session_id)));
        if (sproxy_connection_status == -1) {
            telnet_running = false;
        }

        close(sproxy_connection);
    }
    close(telnet_connection);
}

int main(int argc, char* argv[]) {
#ifdef CLIENT
    if (argc == 4) {
        cproxy(atoi(argv[1]),argv[2],argv[3]);
    } else {
        fprintf(stderr, "Usage: client <ip#> <port#>");
    }
#else
    if(argc == 2){
       sproxy(atoi(argv[1]));
    }else{
       fprintf(stderr,"Error missing sport arg");
    }
#endif
    return 0;
}
