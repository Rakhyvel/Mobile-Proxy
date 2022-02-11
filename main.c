#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    printf("%d", port);
   // exit(1);
   // unsigned int buffer_size = 1024;
   // unsigned int buff_pos = 0;
    char buff[1024];
    int acc,b,l,sock;
    //socket in for inet only
    struct sockaddr_in serverAddr, clientAddr;

    sock = socket(PF_INET, SOCK_STREAM, 0);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // htonl INADDR_ANY ;
    serverAddr.sin_port = htons(6202);// added local to hold spot 
    //sock = build_socket();
    //sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0){
      fprintf(stderr,"Unable t create socket");
      exit(1);
    }
    printf("building sock\n");
    //still need to fill out serverAddr
    printf("binding\n");
    b = bind(sock,(struct sockaddr*)&serverAddr, sizeof(serverAddr));
    printf("%d", b);
    if(b < 0){
	fprintf(stderr,"Unable to Bind");
        exit(1);
    }
    printf("listening\n");
    l = listen(sock,5);// pending connections on socket
    if(l < 0){
        fprintf(stderr,"Unable to Listen");
        exit(1);
    }
    printf("reading data");
    while(1){//read infintly
        //accept the conection
        socklen_t client_len;
        client_len  = sizeof(clientAddr);
        printf("accepting ");
        acc = accept(sock,(struct sockaddr *)&clientAddr,&client_len);
        if(acc < 0){
           fprintf(stderr,"Unable to accept connection");
           exit(1);
        }
        int len; //, size;
       // int sumBit = 0;
        
	//iwhile(sumBit < 4){
        //    len = recv(acc,buff,sizeof(buff), 0);
        //    sumBit = len + sumBit;
       // }
        while((len = recv(acc,buff,sizeof(buff),0))){//read in from client
            //ntohs() 
            printf("rev -> %d\n",len);
            printf("%s\n",buff);
       }

    }

   free(buff);

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

int main(int argc, char* argv[]) {
#ifdef CLIENT
    printf("I am a client\n");
    client(stdin);
#else
    printf("I am a server\n");
    if(argc == 2){
       server(atoi(argv[1]));
    }else{
       fprintf(stderr,"Error missing sport arg");
    }
#endif
    return 0;
}
