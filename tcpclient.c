/*
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define BUFSIZE 1024

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void estimiating(double *y_s, double *y_var, double *y_i, double *y_up){
	double alpha = 0.1;
    double beta = 0.2;
	double kappa = 0.5;
	*y_var = (1 - beta) * (*y_var) + beta * abs(*y_s - *y_i);
	*y_s = (1 - alpha) * (*y_s) + alpha * (*y_i);
	*y_up = *y_s + kappa * (*y_var);
}

double get_offset(){
    const char *filepath = "result.txt";
    int fd = open(filepath, O_RDONLY, (mode_t)0600);
    
    if (fd == -1)
    {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
    
    struct stat fileInfo = {0};
    
    if (fstat(fd, &fileInfo) == -1)
    {
        perror("Error getting the file size");
        exit(EXIT_FAILURE);
    }
    
    if (fileInfo.st_size == 0)
    {
        fprintf(stderr, "Error: File is empty, nothing to do\n");
        exit(EXIT_FAILURE);
    }
    
    printf("File size is %ji\n", (intmax_t)fileInfo.st_size);
    
    double *map = mmap(0, fileInfo.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
    {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }
    
    double offset = map[0];
    // Don't forget to free the mmapped memory
    if (munmap(map, fileInfo.st_size) == -1)
    {
        close(fd);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }
    // Un-mmaping doesn't close the file, so we still need to do that.
    close(fd);
    return offset;
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    double y_s, y_var, y_up;
    
    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    
    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    
    /* connect: create a connection with the server */
    if (connect(sockfd, &serveraddr, sizeof(serveraddr)) < 0)
        error("ERROR connecting");
    
    int timer = 0;
    int tag;
    while(timer < 60) {
        char* f_latency = "latency.txt";
        FILE *fp_latency = fopen(f_latency, "a+");
        char* f_name = "send.png";
        int size;
        FILE *fp = fopen(f_name, "rb");

        if(fp == NULL){
            error("ERROR open file");
        }

        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        printf("Image size is: %d\n", size);
        
        //n = write(sockfd, &timer, sizeof(int));
        //if (n < 0)
        //    error("ERROR writing to socket");

        n = write(sockfd, &size, sizeof(int));
        if (n < 0)
            error("ERROR writing to socket");

        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        
        bzero(buf, BUFSIZE);
        int f_block_sz;
        while((f_block_sz = fread(buf, sizeof(char), BUFSIZE, fp)) > 0){
            /* send the message line to the server */
            n = write(sockfd, buf, f_block_sz);
            if (n < 0)
                error("ERROR writing to socket");
            
            /* print the server's reply */
            bzero(buf, BUFSIZE);
        }
        
        uint64_t t_finish;
        //n = read(sockfd, &tag, sizeof(int));
        n = read(sockfd, &t_finish, sizeof(uint64_t));

        //printf("ieration %d\n", tag);
        printf("t_finish: %llu\n", t_finish);
        
        double offset = get_offset();
        
        double latency = t_finish/1000000.0 - ((double)tv_start.tv_sec - tv_start.tv_usec/1000000.0) - offset;
       
        if(timer == 0){
        y_s = latency / 4.0;
        y_var = latency / 4.0;
        y_up = latency / 4.0;
        } else {
        estimiating(&y_s,  &y_var, &latency, &y_up);
        }

        fprintf(fp_latency, "%f %f %f %f\n", latency, y_s, y_var, y_up);
        printf("Latency is %f, y_s is %f, y_var is %f, y_up is %f\n", latency, y_s, y_var, y_up);

        fclose(fp);
        sleep(1);
        timer++;
    }
    close(sockfd);
    printf("Transmission finished!\n");
    return 0;
}