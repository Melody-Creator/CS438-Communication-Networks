/* 
 * File:   receiver_main.c
 * Author: Ever Wong
 *
 * Created on 2022/10/5
 * Use UDP to implement TCP basic features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BUF_SIZE (1400 * 1400)
#define HEADER_SIZE 11
#define MSS 1400
#define FILE_SIZE (MSS - HEADER_SIZE)

#define offset(x) (x % MAX_BUF_SIZE)

enum TCP_PHASE  { SYN1, SYN0, FIN, FIN_ACK };

struct sockaddr_in si_me, si_other;
int sockfd, slen;
struct timeval time_out;

unsigned long long int recv_base, bytes_received;
char buffer[MAX_BUF_SIZE + 1];      // recv buffer
int rcvd_seq[MAX_BUF_SIZE + 1];     // to mark whether a seq is recvd

void diep(char *s) {
    perror(s);
    exit(1);
}

/*
 * byte 0: phase
 * byte 1-8 seq
 * byte 9-10: numbytes (1-1024)
 */
void packHeader(int phase, unsigned long long int ack, char* header) {

    header[0] = (char)phase;

    for (int i = 8; i >= 1; i--) {
        header[i] = (char)(ack & 0xFF);
        ack >>= 8;
    }
}

void unpackHeader(int* phase, unsigned long long int* seq, int* numbytes, char* buf) {

    *phase = (int)(unsigned char)buf[0];

    *seq = 0;
    for (int i = 1; i <= 8; i++) 
        (*seq) = ((*seq) << 8) | (unsigned long long int)(unsigned char)buf[i];


    *numbytes = 0;
    for (int i = 9; i <= 10; i++) 
        (*numbytes) = ((*numbytes) << 8) | (int)(unsigned char)buf[i];
}


/* 
 * Three-way handshake
 */

void tcpConnect() {

    unsigned long long int seq, ack;
    int phase, numbytes;

    char header[HEADER_SIZE + 1], connect_buf[HEADER_SIZE + 1];
    bzero(connect_buf, sizeof(connect_buf));
    bzero(header, sizeof(header));

    // receiver the first handshake
    if (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) 
        diep("Connection set up failed!");
    
    unpackHeader(&phase, &seq, &numbytes, connect_buf);
    ack = seq + 1;

    packHeader(SYN1, ack, header);

    // second handshake
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    time_out.tv_sec = 0;
    time_out.tv_usec = 50 * 1000;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));  // set timeout

    while (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) {
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
    }
    puts("TCP connection is set up! Ready to receive!");
}


/*
 * TCP packets receiving process
 */

void tcpRecv(char* destinationFile) {

    puts("Start receiving ...");
    int phase, numbytes;
    unsigned long long int seq;
    char header[HEADER_SIZE + 1], recv_buf[MSS + 1];
    bzero(recv_buf, sizeof(recv_buf));
    bzero(header, sizeof(header));

    FILE *fptr = NULL;
    if ((fptr = fopen(destinationFile, "wb")) == NULL) 
        diep("Failed to open destinationFile\n");
    
    time_out.tv_sec = 0;
    time_out.tv_usec = 0;    // cancel the time out mechanic
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    while (recvfrom(sockfd, recv_buf, MSS, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) >= 0) {

        unpackHeader(&phase, &seq, &numbytes, recv_buf);

        if (phase == FIN)  break;  // receive a FIN
        if (seq < recv_base || rcvd_seq[offset(seq)] == 1)  continue;   // duplicate packets

        memcpy(buffer+offset(seq), recv_buf, sizeof(char) * MSS);
        rcvd_seq[offset(seq)] = 1;

        // write to file
        if (seq == recv_base) {
            while (rcvd_seq[offset(recv_base)] != 0) {
                unpackHeader(&phase, &seq, &numbytes, buffer+offset(recv_base));

                if (fwrite(buffer+offset(recv_base)+HEADER_SIZE, sizeof(char), numbytes, fptr) < numbytes) 
                    diep("Failed to write into destinationFile\n");

                rcvd_seq[offset(recv_base)] = 0;
                recv_base += 1ULL * MSS;
                bytes_received += 1ULL * numbytes;
            }
        }  
        
        packHeader(SYN0, recv_base, header);

        // send ack
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
    }

    fclose(fptr);
}


/*
 * TCP close, 4 time hand-waves
 */
void tcpDisconnect() {

    char header[HEADER_SIZE + 1], connect_buf[HEADER_SIZE + 1];
    bzero(connect_buf, sizeof(connect_buf));
    bzero(header, sizeof(header));

    packHeader(FIN_ACK, 0, header);

    // second wave (FIN receive in tcpRecv())
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    packHeader(FIN, 0, header);

    // third wave
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));  // set timeout
    while (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) {
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
    }

    puts("TCP connection tears down! (server side)");
}


void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(sockfd, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */    
    tcpConnect();
    tcpRecv(destinationFile);
    tcpDisconnect();

    close(sockfd);
	printf("%llu bytes received in %s.\n", bytes_received, destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
    return 0;
}

