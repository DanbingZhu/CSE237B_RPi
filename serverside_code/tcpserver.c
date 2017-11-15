/*
 * tcpserver.c - A simple TCP echo server
 * usage: tcpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFSIZE 1024*4

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    char buf[BUFSIZE]; /* message buffer */
    char *hostaddrp; /* dotted decimal host addr string */
    int optval; /* flag value for setsockopt */
    int n; /* message byte size */
    
    /*
     * check command line arguments
     */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);
    
    /*
     * socket: create the parent socket
     */
    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    if (parentfd < 0)
        error("ERROR opening socket");
    
    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    
    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    
    /* this is an Internet address */
    serveraddr.sin_family = AF_INET;
    
    /* let the system figure out our IP address */
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    /* this is the port we will listen on */
    serveraddr.sin_port = htons((unsigned short)portno);
    
    /*
     * bind: associate the parent socket with a port
     */
    if (bind(parentfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR on binding");
    
    /*
     * listen: make this socket ready to accept connection requests
     */
    if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */
        error("ERROR on listen");
    
    /*
     * main loop: wait for a connection request, echo input line,
     * then close connection.
     */
    clientlen = sizeof(clientaddr);
        
    /*
     * accept: wait for a connection request
     */
    childfd = accept(parentfd, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
    if (childfd < 0)
        error("ERROR on accept");

    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
        error("ERROR on inet_ntoa\n");
    
    while(1) {
        printf("Reading Picture Size\n");
        int size, tag = 0;
        
        n = read(childfd, &size, sizeof(int));
        if (n < 0)
            error("ERROR reading from socket");
        printf("Size: %d\n", size);
        
        //n = read(childfd, &tag, sizeof(int));
        //if (n < 0)
        //    error("ERROR reading from socket");
        //printf("Tag: %d\n", tag);
        
        char* f_name = "receive.png";
        FILE *fp = fopen(f_name, "wb");
        if(fp == NULL) error("ERROR open file");
        /*
         * read: read input string from the client
         */
        bzero(buf, BUFSIZE);
        int recConunt = 0;
        while(recConunt < size){
            n = read(childfd, buf, BUFSIZE);
            if (n < 0)
                error("ERROR reading from socket");
            int write_sz = fwrite(buf, sizeof(char), n, fp);
            if(write_sz < n){
                error("ERROR write file");
            }
            recConunt += n;
            bzero(buf, BUFSIZE);
            
        }
        
        struct timeval tv_finish;
        gettimeofday(&tv_finish, NULL);
        
        printf("RECe Sec Usec: %ld， %d\n", tv_finish.tv_sec, tv_finish.tv_usec);
        uint64_t t_finish = (uint64_t)tv_finish.tv_sec * 1000000 + tv_finish.tv_usec;
        printf("t_finish: %llu\n", t_finish);
        
       // n = write(childfd, &tag, sizeof(int));
       // if (n < 0)
       //     error("ERROR writing to socket");
        n = write(childfd, &t_finish, sizeof(uint64_t));
        if (n < 0)
            error("ERROR writing to socket");
        
        printf("ok!\n");
        fclose(fp);
    }
    
    close(childfd);
    return 0;
}