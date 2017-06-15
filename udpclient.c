/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

/* Standard NTP packet, not necessary though */
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

/* endpoint */
typedef struct{
    unsigned type : 2; // 2 bits, 0 - lowpoint, 1 - midpoint, 2 - highpoint
    double value; // The value of the end point;
} ntp_point;

typedef struct{
    double l;
    double u;
    double deviation;
} ntp_survivor;

/* compare function for qsort in selection algorithm*/
int compare_select(const void *p1, const void *p2) {
    ntp_point *c1 = (ntp_point *) p1;
    ntp_point *c2 = (ntp_point *) p2;
    if(c1->value < c2->value)
	return -1;
    else if(c1->value > c2->value)
	return 1;
    else
	return 0;
}

/* compare function for qsort in clustering algorithm */
int compare_cluster(const void *s1, const void *s2) {
    ntp_survivor *c1 = (ntp_survivor *) s1;
    ntp_survivor *c2 = (ntp_survivor *) s2;
    if(c1->deviation < c2->deviation)
        return -1;
    else if(c1->deviation > c2->deviation)
        return 1;
    else
        return 0;
}

/* function used to get deviation of each target survivor */
double find_deviation(ntp_survivor *s,int target, int len){
    int i;
    double sum = 0;
    for(i = 0; i < len; i++){
        sum += pow(((s[target].l+s[target].u)/2.0 - (s[i].l+s[i].u)/2.0), 2);
    }
    
    return sqrt(sum/(len - 1));
}



/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr; //Server address data structure
    struct hostent *server; // Server data structure
    char *hostname;
    char temp[60];
    int i, j;
    int flag;
    // parameters for the selection algorithm
    double l, u;
    int m, s, f, d, c;
    // parameter for the clustering algorithm
    int MIN, len, victim;
    // parameters for the combining algorithm
    double y,z;
    // final estimate
    double final_estimate;
    // Output file
    
    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if (sockfd < 0)
        error("ERROR opening socket");
    
    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname); // Convert URL to IP
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    
    /* build the server's Internet address */
    // Zero out the server address structure
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    // Copy the server's IP address to the server address structure
    bcopy((char *)server->h_addr,
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    // Convert the port number integer to network big-endian style and save it to the server address structure.
    serveraddr.sin_port = htons(portno);
    

    printf("please enter desired value of m:");
    fgets(temp, 60, stdin);
    m = atoi(temp);
    printf("m is set to %d\n", m);
        
    printf("please enter desired value of MIN(less than s):");
    fgets(temp, 60, stdin);
    MIN = atoi(temp);
    printf("MIN is set to %d\n", MIN);

    /* Set up an array of endpoints to store lowpoint, midpoint and highpoint*/
    ntp_point endpoints[3*m];
    ntp_survivor candidates[m];
    ntp_survivor survivors[m];
    /* Initializing the NTP packet */
    ntp_packet packet = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Open the file
    
    while(1){
    i = 0;
    flag = 1;
    while(i < m){
        memset( &packet, 0, sizeof( ntp_packet ) );
        
        struct timeval tv;
        gettimeofday(&tv, NULL);
        packet.origTm_s = (uint32_t)tv.tv_sec;
        packet.origTm_f = (uint32_t)tv.tv_usec;
        

        /* send the message to the server */
        serverlen = sizeof(serveraddr);
        n = sendto(sockfd, (char *) &packet, sizeof(packet), 0, &serveraddr, serverlen);
        if (n < 0)
            error("ERROR in sendto");
        /* print the server's reply */
        n = recvfrom(sockfd, (char *) &packet, sizeof(packet), 0, &serveraddr, &serverlen);
        if (n < 0)
            error("ERROR in recvfrom");
	
        //printf("Echo from server: %u, %u, %u,\n %u, %u, %u\n", packet.origTm_s,packet.origTm_f, packet.rxTm_s, packet.rxTm_f, packet.txTm_s, packet.txTm_f);
        
        gettimeofday(&tv, NULL);
        packet.refTm_s = (uint32_t)tv.tv_sec;
        packet.refTm_f = (uint32_t)tv.tv_usec;
        //printf("T_dst is: %u, %u\n", packet.refTm_s, packet.refTm_f);

        double t_org = (double)packet.origTm_s + packet.origTm_f/1000000.0;
        double t_rec = (double)packet.rxTm_s + packet.rxTm_f/1000000.0;
        double t_xmt = (double)packet.txTm_s + packet.txTm_f/1000000.0;
        double t_dst = (double)packet.refTm_s + packet.refTm_f/1000000.0;

        //printf("t_org: %f, t_rec: %f, t_xmt: %f, t_dst: %f\n", t_org, t_rec, t_xmt, t_dst);

        double RTT = (t_dst - t_org) - (t_xmt - t_rec);
        double offset = ((t_rec - t_org) + (t_xmt - t_dst))/2;
        double lowbound = offset - RTT/2;
        double highbound = offset + RTT/2;
        //printf("RTT: %f s, Offset: %f s\n", RTT, offset);
        //printf("Bound of estimate: [%f, %f]\n", lowbound, highbound);
        candidates[i].l = lowbound;
        candidates[i].u = highbound;
        endpoints[i].type = 0;
        endpoints[i].value = lowbound;
        endpoints[i+m].type = 1;
        endpoints[i+m].value = offset;
        endpoints[i+2*m].type = 2;
        endpoints[i+2*m].value = highbound;
        i++;
    }
    
    /* 
     * Start of selection algorithm
     */
    // Sort the endpoints
    qsort(endpoints, 3*m, sizeof(ntp_point), compare_select);
    
    //for(i = 0; i < 3*m; i++){
    //    printf("type: %u, value: %f\n", endpoints[i].type, endpoints[i].value);
    //}
    
    f = 0; // set the number of flasetickers to zero
    l = 0;
    u = 0;
    while(1){
        d = 0;
        c = 0;
        for(i = 0; i < 3*m; i++){
            if(endpoints[i].type == 0)
                c++;
            else if(endpoints[i].type == 2)
                c--;
            else
                d++;
            
            if(c >= (m - f)){
                l = endpoints[i].value;
                break;
            }
        }
        
	c = 0;
        for(i = 0; i < 3*m; i++){
            if(endpoints[3*m - 1 - i].type == 2)
                c++;
            else if(endpoints[3*m - 1 - i].type == 0)
                c--;
            else
                d++;
            
            if(c >= (m - f)){
                u = endpoints[3*m - 1 - i].value;
                break;
            }
        }
        
        if(d <= f && l < u){
            //fprintf(fp, "[%f, %f]\n", l , u);
	    printf("[%f, %f]\n", l , u);
            break;
        } else{
            f++;
            if(f >= m/2){
                //printf("Failure;a majority clique could not be found..\n");
                flag = 0;
		break;
            }
        }
    }
    
    if(flag){
    len = 0;
        
    for(i = 0; i < m; i++){
        if((candidates[i].l + candidates[i].u >= 2*l) && (candidates[i].l + candidates[i].u) <= 2*u){
            survivors[len].l = candidates[i].l;
            survivors[len].u = candidates[i].u;
            len++;
        }
	survivors[i].deviation = 0;
    }
    
    
    //for(i = 0; i < s; i ++){
    //   printf("survivor %d: l %f, u %f\n", i, survivors[i].l, survivors[i].u);
    //}
    /* End of selection algorithm */
    
    /*
     * Start of clustering algorithm
     */
    victim = 0;
    while(len > MIN){
        for(i = 0; i < len; i++){
            survivors[i].deviation = find_deviation(survivors, i, len);
            //printf("deviation for survivor %d is %f\n", i, survivors[i].deviation);
        }
        qsort(survivors, len, sizeof(ntp_survivor), compare_cluster);
        //for(i = 0; i < len; i++){
            //printf("deviation for survivor %d is %f\n", i, survivors[i].deviation);
        //}
	len--;
    }
    
    //printf("The list after clustering algorithm is\n");
    //for(i = 0; i < len; i++){
    //    printf("%d, l %f, u %f\n", i, survivors[i].l, survivors[i].u);
    //}
    /* End of clustering algorithm */
    
    /* 
     * Start of combining algorithm
     */
    y = 0;
    z = 0;
    
    for(i = 0; i < MIN; i++){
        y += 2/(survivors[i].u - survivors[i].l);
        z += (survivors[i].u + survivors[i].l)/(survivors[i].u - survivors[i].l);
    }
    
    final_estimate = z/y;
    /* End of combining algorithm */

    const char *filepath = "result.txt";
    int fd = open(filepath, O_RDWR | O_CREAT, (mode_t)0600); // | O_TRUNC
    
    if (fd == -1)
    {
        perror("Error opening file for writing");
        exit(EXIT_FAILURE);
    }
    
    size_t datasize = sizeof(double) + 1;

    if (lseek(fd, datasize-1, SEEK_SET) == -1)
    {
        close(fd);
        perror("Error calling lseek() to 'stretch' the file");
        exit(EXIT_FAILURE);
    }

    if (write(fd, "", 1) == -1)
    {
        close(fd);
        perror("Error writing last byte of the file");
        exit(EXIT_FAILURE);
    }
    
    
    // Now the file is ready to be mmapped.
    double *map = mmap(0, sizeof(double), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED)
    {
        close(fd);
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    map[0] = final_estimate;

    // Write it now to disk
    if (msync(map, datasize, MS_SYNC) == -1)
    {
        perror("Could not sync the file to disk");
    }
    
    // Don't forget to free the mmapped memory
    if (munmap(map, datasize) == -1)
    {
        close(fd);
        perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
    }

    close(fd);
    }
    sleep(5);
    }
    
    return 0;
}