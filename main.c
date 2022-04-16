/*
This file contains code for both the client proxy and the server proxy.
At compiletime, define CLIENT to compile the client, and leave it undefined to 
compile the server.

-- Message Forwarding --
The client proxy will connect to telnet on localhost, and then to the server 
proxy. The server proxy will connect to a telnet daemon, and then accept a
client's connection. Any data from telnet will be forwarded to the other proxy,
and any data from the other proxy will be forwarded to telnet.

-- Heartbeats --
A heartbeat message will be sent every second between the two proxies every
second. If there is no heartbeat message and no data for three seconds, the
connection is assumed lost.

-- Message Queueing --
Messages sent between proxies are placed onto a queue. The message at the front
of the queue is sent, and not popped off until an acknowledgement is received
from the other proxy. If there is a disconnect between the proxies, the 
messages will continue to queue, and be sent out once the connection is made 
again. This ensures reliable transfer of data through a disconnect.

-- Session ID --
Each client has a unique 4 byte session ID. When a client proxy connects to a
server proxy, it sends a heartbeat message to the server proxy with it's 
session ID. If the connection is lost, the server remembers the session ID, and
when a new client connects, it will check the session ID. If the session ID 
matches, the connection is resumed as normal. If the session ID does not match,
a new connection is created.

-- Authors --
Joseph Shimel
Austin James Connick
*/

#include "message.h"
#include "queue.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// time since last received message from other proxy
time_t proxytime = 0;
// time since last sent heartbeat
time_t one_sec = 0;

/*
Resets the time when the last received message from other proxy
*/
void start_proxytime() {
    proxytime = time(NULL);
}

/*
Resets the time when the last heartbeat was sent
*/
void start_one_sec(){
    one_sec = time(NULL);
}

/* 
Connects to a server as a client

@param ipText The IP address to connect to as a string
@param portText the port to connect to as a string
@returns The created socket for the server connection
*/
int connect_client(char* ipText, char* portText) {
    struct sockaddr_in addr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("connect_client(): unable to create socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(portText));
    inet_pton(AF_INET, ipText, &addr.sin_addr);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect_client(): unable to connect to socket");
        exit(1);
    }
    return sock;
}

/*
Creates a socket for a server to listen on

@param port The port number for the server
@returns The socket for this server
*/
int connect_server(short port) {
    struct sockaddr_in thisAddr;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    thisAddr.sin_family = AF_INET;
    thisAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    thisAddr.sin_port = htons(port); // added local to hold spot 
    if (sock < 0) {
        fprintf(stderr, "connect_server(): unable to create socket\n");
        exit(1);
    }
    if (bind(sock, (struct sockaddr*)&thisAddr, sizeof(thisAddr)) < 0) {
        fprintf(stderr, "connect_server(): unable to bind\n");
        exit(1);
    }
    return sock;
}

/*
Accepts a connection from a client as a server. Blocks until a connection is accepted.

@param sock The socket for this server
@returns The socket for the client connection
*/
int accept_server(int sock) {
    struct sockaddr_in addr;
    socklen_t addrLen  = sizeof(addr);

    if (listen(sock, 5) < 0) {
        fprintf(stderr, "accept_server(): unable to listen\n");
        exit(1);
    }
    
    int serverCon = accept(sock, (struct sockaddr*)&addr, &addrLen); // blocks!
    return serverCon;
}

/*
Listens to both telnet connection and proxy connection simultaneosly. If there 
is input from telnet, forwards to proxy, if there is input from proxy, forwards
to telnet. Finally, sends a heartbeat message if one second has passed.

@param telnet_connection The socket for the telnet connection
@param proxySock The socket for the proxy connection
@param session_id Session ID for this proxy
@returns 0 if OK, 1 if proxy is closed, -1 if telnet is closed
*/
int is_closed(int telnet_connection, int proxySock, int session_id) {
    char telnet_buff[1024]; // buffer for incoming telnet data
    const int TELNET_BUFF_MAX_LEN = 1024;

    // Set up select() file descriptor set
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    fd_set readfds;
    FD_SET(telnet_connection, &readfds);
    FD_SET(proxySock, &readfds);

    int n = (telnet_connection > proxySock ? telnet_connection : proxySock) + 1;
    if (select(n, &readfds, NULL, NULL, &tv) < 0){
        fprintf(stderr, "Error in select\n");
        exit(1);
    }

    // if input from telnet, send to proxy
    if (FD_ISSET(telnet_connection, &readfds)) {
        memset(telnet_buff, 0, TELNET_BUFF_MAX_LEN);
        int telnet_buff_len = recv(telnet_connection, telnet_buff, TELNET_BUFF_MAX_LEN, 0);
        if (telnet_buff_len <= 0) {
            return -1;
        }
        push_msg(DATA, session_id, telnet_buff, telnet_buff_len);
    }
    // if input from proxy, send to telnet
    if (FD_ISSET(proxySock, &readfds)) {
        char* proxy_data;
        Header header = recv_header(proxySock, &proxy_data);
        switch(header.type) {
        case DATA:
            send_raw(telnet_connection, proxy_data, header.length);
            free(proxy_data);
            send_header(proxySock, NULL, (Header){ACK, 0, session_id});
            break;
        case ACK:
            pop_front();
            break;
        case HEARTBEAT:
            break;
        case END: // damn you aint kiddin brether
            return 1;
        }
        start_proxytime(); //reset time out
    }

    // Send heartbeat if one second has past
    if (time(NULL) - one_sec > 1) {
        send_header(proxySock, NULL, (Header){HEARTBEAT, 0, session_id}); 
        start_one_sec();
    }

    // Close connection if nothing has been received for 3 seconds
    if (time(NULL) - proxytime > 3) {
        return 1;
    } else {
        send_front(proxySock); // Attempt to send front of queue
        return 0;
    }
}

/*
Connects to a telnet daemon, and then accept a client's connection. Any data 
from telnet will be forwarded to the client proxy, and any data from the client
proxy will be forwarded to telnet.

@param port Port number to accept connections on
*/
void sproxy(int port) {
    // Connect to telnet daemon
    int telnetDeamon_connection = connect_client("127.0.0.1", "23");
    int serverSock = connect_server(port);

    bool telnet_running = true;
    int ID = -1;
    
    while (telnet_running) {
        // Accept cproxy connection
        int cproxy_connection = accept_server(serverSock);

        // Receive heartbeat from cproxy with session ID, reset if session ID changed
        Header header = recv_header(cproxy_connection, NULL);
        if (header.session_id != ID) {
            ID = header.session_id;
            close(telnetDeamon_connection);
            telnetDeamon_connection = connect_client("127.0.0.1", "23");
            reset_queue();
        }

        // Reset timers and queue ack await status
        start_one_sec();
        start_proxytime();
        reset_await_status();

        // cproxy forward loop
        int cproxy_connection_status;
        while (!(cproxy_connection_status = is_closed(telnetDeamon_connection, cproxy_connection, ID)));
        if (cproxy_connection_status == -1) {
            telnet_running = false;
        }

        close(cproxy_connection);
    }
    close(telnetDeamon_connection);
    close(serverSock);
}

/*
Accepts a telnet connection, and then connects to a server proxy. Any data from
telnet will be forwarded to the server proxy, and any data from the server 
proxy will be forwarded to telnet.

@param port The port number for telnet
@param ipText The ip address of the server proxy as a string
@param portText The port of the server proxy as a string
*/
void cproxy(int port, char* ipText , char* portText) {
    // Create telnet server socket
    int telnet_sock = connect_server(port);
    int telnet_connection = accept_server(telnet_sock);
    int session_id = time(NULL);

    bool telnet_running = true;
    while (telnet_running) {
        // Connect to sproxy socket
        int sproxy_connection = connect_client(ipText, portText);
        
        // Send heartbeat with session ID
        send_header(sproxy_connection, NULL, (Header){HEARTBEAT, 0, session_id});

        // Reset timers
        start_one_sec();
        start_proxytime();

        // sproxy forward loop
        int sproxy_connection_status;
        while (!(sproxy_connection_status = is_closed(telnet_connection, sproxy_connection, session_id)));
        if (sproxy_connection_status == -1) {
            telnet_running = false;
        }

        close(sproxy_connection);
    }
    close(telnet_sock);
    close(telnet_connection);
}

/*
starts either the server or the client depending on if CLIENT is defined at 
compile time
*/
int main(int argc, char* argv[]) {
#ifdef CLIENT
    if (argc == 4) {
        cproxy(atoi(argv[1]),argv[2],argv[3]);
    } else {
        fprintf(stderr, "usage: cproxy <telnet port> <server ip> <server port>\n");
    }
#else
    if(argc == 2){
       sproxy(atoi(argv[1]));
    }else{
       fprintf(stderr,"usage: sproxy <server port>\n");
    }
#endif
    return 0;
}
