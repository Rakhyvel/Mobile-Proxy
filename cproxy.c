
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

#define MAX(x, y) (x > y ? x : y)

/*
 * Takes in a socket, some data, and the length of the data and ensures that
 * the entire chunk of data is transfered
 */
static void send_data(int sock, char* data, int num_bytes) {
    int bytes_sent;
    do {
        bytes_sent = send(sock, data, num_bytes, 0);
        if (bytes_sent == -1) {
            perror("failed send data:");
            exit(1);
        } else {
            data += bytes_sent;
            num_bytes -= bytes_sent;
        }
    } while(num_bytes > 0);
}
/* 
 * Connects to sproxy, then awaits a connection from telnet and forwards data
 * between telnet and sproxy
 */
void cproxy(int port, char* ipText , char* portText) {
    char buff[1024];
    char buff2[1024];
    int MAX_LEN = 1024;
    memset(buff2, 0, MAX_LEN);
    struct sockaddr_in sproxyAddr; // address of sproxy
    struct sockaddr_in cproxyAddr; // self address
    struct sockaddr_in telnetAddr; // address for telnet connection

    // Connect to sproxy
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

    while (1) {
        // Accept new telnet connection
        socklen_t telnetLen;
        telnetLen  = sizeof(telnetAddr);
        int telnetCon = accept(telnetSock, (struct sockaddr*)&telnetAddr, &telnetLen);
        printf("telnet connection accepted\n");

        int rest = 1;
        while (rest) {
            struct timeval tv;
            fd_set readfds;

            FD_SET(telnetCon, &readfds);
            FD_SET(sproxySock, &readfds);
            int n = MAX(telnetCon, sproxySock) + 1;

            tv.tv_sec = 10;
            tv.tv_usec = 500000;
            
            int rv = select(n, &readfds, NULL, NULL, &tv);
            if(rv < 0){
                fprintf(stderr, "Error in select");
                exit(1);
            }

            // if input from telnet, send to sproxy
            if (FD_ISSET(telnetCon, &readfds)) {
                printf("recv data from telnet...\n");
                memset(buff, 0, MAX_LEN);
                int rev = recv(telnetCon, buff, MAX_LEN, 0);
                if (rev <= 0) {
                    break;
                }
                printf("telnet data: %s\n", buff);
                send_data(sproxySock, buff, rev);
            }
            // if input from sproxy, send to telnet
            if (FD_ISSET(sproxySock, &readfds)) {
                printf("recv data from sproxy...\n");
                memset(buff2, 0, MAX_LEN);
                int rev2 = recv(sproxySock, buff2, MAX_LEN, 0);
                if (rev2 <= 0) {
                    break;
                }
                printf("sproxy data: %s\n", buff2);
                send_data(telnetCon, buff2, rev2);
            }
        }
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////  
int main(int argc, char* argv[]) {
    if(argc == 4){
       cproxy(atoi(argv[1]),argv[2],argv[3]);
    }else{
       fprintf(stderr,"Error missing sport arg");
    }
    return 0;
}