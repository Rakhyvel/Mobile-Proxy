
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


// static bool recv_data(int sock, char* data, int num_bytes) {
//     int bytes_recv;
//     do {
//         bytes_recv = recv(sock, data, num_bytes, 0);
//         if (bytes_recv <= 0) {
//             return true;
//         } else {
//             data += bytes_recv;
//             num_bytes -= bytes_recv;
//         }
//     } while(num_bytes > 0);
//     return false;
// }

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
 * Awaits a connection from cprocy when connected conncts to telnet deamon
 */
void sproxy(int port) {
    char buff[1024];
    char buff2[1024];
    int MAX_LEN = 1024;
    int acc, b, l, sock;
    struct sockaddr_in serverAddr, clientAddr;

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
/////////////////////////////////////////////////////////////////////////////////////////////////////telnet daemon//
    printf("looking for telnet\n");
    char ipText[] = "127.0.0.1";
    char portText[] = "23";
    //int c; // Char retrieved from input stream, will be EOF at end of file
    //unsigned int buffer_pos = 0; // cursor into buffer
    //int net_buffer_pos; // big endian version of buffer_pos
    //char buffer[1024]; // character buffer to store input
    struct sockaddr_in serverAddr2; // address to connect to
    int sockDeamon; // socket to send to
    //int end = 1;
    // Create socket 'sock'
    sockDeamon = socket(PF_INET, SOCK_STREAM, 0);

    if (sockDeamon == -1) {
        // err
        perror("client failed creating socket");
        exit(1);
    }

    // Connect socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(portText));
    inet_pton(AF_INET, ipText, &serverAddr2.sin_addr);
    if(connect(sockDeamon, (struct sockaddr*)&serverAddr2, sizeof(serverAddr2)) == -1) {
        perror("client failed connecting socket");
        exit(1);
    }
    printf("created telnet socket\n");
    int rest = 1;
    int n , rv;
    struct timeval tv;
    fd_set readfds;

    FD_SET(sock, &readfds);
    FD_SET(sockDeamon, &readfds);
    if(sock > sockDeamon) n = sock + 1;
    else n = sockDeamon + 1;

    tv.tv_sec = 10;
    tv.tv_usec = 500000;

    while(rest){
        printf("loop\n");
        tv.tv_sec = 10;
        tv.tv_usec = 500000;
        rv = select(n,&readfds,NULL,NULL,&tv);
        printf("select\n");
        if(rv < 0){
            fprintf(stderr,"Error in select");
            exit(1);
        }
        int rev, rev2;
        printf("rev\n");
        if(FD_ISSET(sock, &readfds)){
            rev = recv(acc,buff,MAX_LEN,0);
            if(rev < 0){
                printf("break\n");
                break;
            }
            send_data(sockDeamon, buff, rev);
        }
        printf("rev2\n");
        if(FD_ISSET(sockDeamon,&readfds)){
            rev2 = recv(sockDeamon,buff2,MAX_LEN,0);
            if(rev2 < 0){
                printf("break2\n");
                break;
            }
            send_data(sock, buff2, rev2);
        }
      }
      printf("end loop\n");
    }
///////////////////////////////////////////////////////////////////////////////////////////////////////  
int main(int argc, char* argv[]) {
    if(argc == 2){
       sproxy(atoi(argv[1]));
    }else{
       fprintf(stderr,"Error missing sport arg");
    }
    return 0;
}