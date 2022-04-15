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
time_t one_sec = 0;
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

void start_heart_one_sec(){
    time_t sec_one;
    sec_one = time(NULL);
    one_sec = sec_one;
}


int time_from_last(){
    time_t sec_one_c;
    sec_one_c = time(NULL);
    printf("time %d - %d\n",(int)sec_one_c,(int)one_sec);
    return sec_one_c - one_sec;

}


void send_heart_beat(Header header, int sock, int session_id) {
    send_header(sock, NULL, header); 
}

int test_heart_beat(int sock, int session_id) {
    Header header;
    header.type = HEARTBEAT;
    header.session_id = session_id;
    header.length = 0;
    header.msg_num = 0;
/*
    if (heart_beat_count_fails > 3) {
        heart_beat_count_fails = 0;
        return 1;
    }
*/
    printf("time gl hb t1 = %d t2 = %d\n",(int)proxytime,(int)one_sec);
    if(time_from_last() > 1){//sends heart every 
        printf("time from hb%d\n",time_from_last());
        send_heart_beat(header,sock,session_id);
        start_heart_one_sec();
    } 
    if(time_from_heart() > 3){
        printf("3sec stop\n");
        printf("real time%d\n",time_from_heart());
        return 1;
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
    struct sockaddr_in thisAddr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    thisAddr.sin_family = AF_INET;
    thisAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    thisAddr.sin_port = htons(port); // added local to hold spot 
    if (sock < 0) {
        fprintf(stderr, "Unable to create socket");
        exit(1);
    }
    if (bind(sock, (struct sockaddr*)&thisAddr, sizeof(thisAddr)) < 0) {
        fprintf(stderr, "Unable to Bind");
        exit(1);
    }
    return sock;
}

int accept_server(int sock) {
    struct sockaddr_in addr;
    socklen_t addrLen  = sizeof(addr);

    if (listen(sock, 5) < 0) {
        fprintf(stderr, "Unable to Listen");
        exit(1);
    }
    
    int serverCon = accept(sock, (struct sockaddr*)&addr, &addrLen); // blocks!
    return serverCon;
}

// returns 0 if good, 1 if sproxy is closed, -1 if telnet is closed
int is_closed(int telnet_connection, int proxySock, int session_id) {
    printf("enter is_closed\n");
    char buff[1024];
    int MAX_LEN = 1024;

    struct timeval tv;
    tv.tv_sec = 0;
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
        printf("telnet->proxy\n");
        memset(buff, 0, MAX_LEN);
        int rev = recv(telnet_connection, buff, MAX_LEN, 0);
        printf("recv from buff: %s", buff);
        if (rev <= 0) {
            return -1;
        }
        push_msg(DATA, session_id, buff, rev);
    }
    // if input from proxy, send to telnet
    if (FD_ISSET(proxySock, &readfds)) {
        printf("proxy->telnet\n");
        char* buff2;
        Header header = recv_header(proxySock, &buff2);
        switch(header.type) {
        case DATA:
            printf("data\n");
            send_raw(telnet_connection, buff2, header.length);
            free(buff2);
            header = (Header){ACK, 0};
            send_header(proxySock, NULL, header);
            start_time();//reset time out 
            break;
        case ACK:
            printf("ack\n");
            pop_front();
            start_time();//reset time out 
            break;
        case HEARTBEAT:
            printf("hb\n");
            //heart_beat_count_fails = 0;
            start_time();//reset time out
            break;
        case END:
            printf("end\n");
            return 1;
        }
    }
    if (test_heart_beat(proxySock, session_id)) {
        printf("hb end\n");
        return 1;
    }
    
    send_front(proxySock);
    return 0;
}


void sproxy(int port) {
    printf("sproxy\n");
    // Connect to telnet daemon
    int telnetDeamon_connection = connect_client("127.0.0.1", "23");
    int serverSock = connect_server(port);

    bool telnet_running = true;
    int ID = -1;
    
    while (telnet_running) {
        printf("while telnet running\n");
        int cproxy_connection = accept_server(serverSock);

        Header header = recv_header(cproxy_connection, NULL);
        if (header.session_id != ID) {
            printf("Session has changed!");
            ID = header.session_id;
            reset_queue();
        }

        start_heart_one_sec();//rerest one sec timer
        start_time();//reset time out
        int cproxy_connection_status;
        while (!(cproxy_connection_status = is_closed(telnetDeamon_connection, cproxy_connection, ID)));
        if (cproxy_connection_status == -1) {
            printf("telnet closed\n");
            telnet_running = 0;
        }
        close(cproxy_connection);
        reset_await_status();
    }
    close(telnetDeamon_connection);
    close(serverSock);
}

void cproxy(int port, char* ipText , char* portText) {
    printf("cproxy\n");
    // Create telnet server socket
    int telnet_sock = connect_server(port);
    int telnet_connection = accept_server(telnet_sock);

    bool telnet_running = true;
    while (telnet_running) {
        printf("while telnet running\n");
        int session_id = 1234;

        // Connect to sproxy socket
        int sproxy_connection = connect_client(ipText, portText);

        // sproxy send loop
        send_header(sproxy_connection, NULL, (Header){HEARTBEAT, 0});
        start_heart_one_sec();//rerest one sec timer
        start_time();//reset time out
        int sproxy_connection_status;
        while (!(sproxy_connection_status = is_closed(telnet_connection, sproxy_connection, session_id)));
        if (sproxy_connection_status == -1) {
            printf("telnet closed\n");
            telnet_running = false;
        }

        close(sproxy_connection);
    }
    close(telnet_sock);
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
