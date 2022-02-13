#include <stdio.h>
#include <stdlib.h>
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
#endif

static void recv_data(int sock, char* data, int num_bytes) {
    int bytes_sent;
    do {
        bytes_sent = recv(sock, data, num_bytes, 0);
        if (bytes_sent == -1) {
            perror("client failed sending data");
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
 * Awaits a connection, reads in packet including header and payload, prints out payload
 */
void server(int port) {
    char buff[1024];
    int acc,b,l,sock;
    struct sockaddr_in serverAddr, clientAddr;

    sock = socket(PF_INET, SOCK_STREAM, 0);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    serverAddr.sin_port = htons(port);// added local to hold spot 
    if(sock < 0){
      fprintf(stderr,"Unable t create socket");
      exit(1);
    }
    b = bind(sock,(struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if(b < 0){
	fprintf(stderr,"Unable to Bind");
        exit(1);
    }
    l = listen(sock,5);// pending connections on socket
    if(l < 0){
        fprintf(stderr,"Unable to Listen");
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
        
     int sizeBuff;
     recv_data(sock,(char*)&sizeBuff,4);
     recv_data(sock,buff,ntohl(sizeBuff));
     printf("%d",sizeBuff);
     int i;        
     for(i = 0; i < sizeBuff; i++){
          printf("%c",buff[i]);
     }
   
 close(sock);
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
            perror("client failed sending data");
            exit(1);
        } else {
            data += bytes_sent;
            num_bytes -= bytes_sent;
        }
    } while(num_bytes > 0);
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

    // Pop character from input stream until EOF
    for (;(c = fgetc(in)) != EOF && c != '\n' && buffer_pos < 1024; buffer_pos++) {
        buffer[buffer_pos] = c;
    }
    net_buffer_pos = htonl(buffer_pos);

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

    // Send size and data
    send_data(sock, (char*)&net_buffer_pos, sizeof(net_buffer_pos));
    send_data(sock, buffer, buffer_pos);
}

int main(int argc, char* argv[]) {
#ifdef CLIENT
    printf("I am a client\n");
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
