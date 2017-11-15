/*
 * udpserver.c - A simple UDP echo server
 * usage: udpserver <port>
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

/* NTP packet */
typedef struct{
    //unsigned li   : 2;       // Only two bits. Leap indicator.
    //unsigned vn   : 3;       // Only three bits. Version number of the protocol.
    //unsigned mode : 3;       // Only three bits. Mode. Client will pick mode 3 for client.
    
    //uint8_t stratum;         // Eight bits. Stratum level of the local clock.
    //uint8_t poll;            // Eight bits. Maximum interval between successive messages.
    //uint8_t precision;       // Eight bits. Precision of the local clock.
    
    //uint32_t rootDelay;      // 32 bits. Total round trip delay time.
    //uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
    //uint32_t refId;          // 32 bits. Reference clock identifier.
    
    uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
    uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.
    
    uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
    uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.
    
    uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
    uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.
    
    uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
    uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.
} ntp_packet;                // Total: 384 bits or 48 bytes.

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* port to listen on */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_in clientaddr; /* client addr */
    struct hostent *hostp; /* client host info */
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
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    
    /* setsockopt: Handy debugging trick that lets
     * us rerun the server immediately after we kill it;
     * otherwise we have to wait about 20 secs.
     * Eliminates "ERROR on binding: Address already in use" error.
     */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *)&optval , sizeof(int));
    
    /*
     * build the server's Internet address
     */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)portno);
    
    /*
     * bind: associate the parent socket with a port
     */
    if (bind(sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0)
        error("ERROR on binding");
    
    ntp_packet packet;
    memset( &packet, 0, sizeof( ntp_packet ) );
    /*
     * main loop: wait for a datagram, then echo it
     */
    clientlen = sizeof(clientaddr);
    while (1) {
        /*
         * recvfrom: receive a UDP datagram from a client
         */
        bzero((char *) &packet, sizeof(packet));
        n = recvfrom(sockfd, (char *) &packet, sizeof(packet), 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERROR in recvfrom");
        struct timeval tv;
        // get the server receive time
        gettimeofday(&tv, NULL);
        packet.rxTm_s = (uint32_t)tv.tv_sec;
        packet.rxTm_f = (uint32_t)tv.tv_usec;
        
        /*
         * gethostbyaddr: determine who sent the datagram
         */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostaddrp == NULL)
            error("ERROR on inet_ntoa\n");
        printf("server received %lu/%d bytes: %u, %u, %u, %u, %u, %u\n", sizeof(packet), n, packet.origTm_s, packet.origTm_f, packet.rxTm_s, packet.rxTm_f, packet.txTm_s, packet.txTm_f);
        // get the server transmit time
        gettimeofday(&tv, NULL);
        packet.txTm_s = (uint32_t)tv.tv_sec;
        packet.txTm_f = (uint32_t)tv.tv_usec;
        /*
         * sendto: echo the input back to the client
         */
        n = sendto(sockfd, (char *) &packet, sizeof(packet), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
            error("ERROR in sendto");
    }
}