/* 
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>

#define MAX_BUF_SIZE (1400 * 1400)
#define HEADER_SIZE 11
#define MSS 1400
#define FILE_SIZE (MSS - HEADER_SIZE)

#define offset(x) (x % MAX_BUF_SIZE)
#define min(a, b) ((a) < (b) ? (a) : (b)) 
#define max(a, b) ((a) > (b) ? (a) : (b)) 

#define DEBUG 0

enum TCP_PHASE  { SYN1, SYN0, FIN, FIN_ACK };
enum TCP_STATES { SLOW_START, CONGEST_AVOID, FAST_RECOVERY };

struct sockaddr_in si_other;
int sockfd, slen;

int estimate_RTT = 20, dev_RTT = 5;
struct timeval time_out, base_time, time_rest;

int state, dupACK, cwnd, ssthresh;
unsigned long long int send_base, next_seq, bytes_tranferred;

char buffer[MAX_BUF_SIZE + 1];  // sending buffer
int time_stamp[MAX_BUF_SIZE + 1];

void diep(char *s) {
    perror(s);
    exit(1);
}

/*
 * Get current timestamp in miliseconds, in order to estimate RTT
 */
int clockTick() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    // return the time stamp now in ms
    return (tv.tv_sec - base_time.tv_sec) * 1000 + (tv.tv_usec - base_time.tv_usec) / 1000;
} 

/*
 * byte 0: phase
 * byte 1-8 seq
 * byte 9-10: numbytes (1-1024)
 */
void packHeader(int phase, unsigned long long int seq, int numbytes, char* header) {

    header[0] = (char)phase;

    for (int i = 8; i >= 1; i--) {
        header[i] = (char)(seq & 0xFF);
        seq >>= 8;
    }
   
    for (int i = 10; i >= 9; i--) {
        header[i] = (char)(numbytes & 0xFF);
        numbytes >>= 8;
    }
}

void unpackHeader(unsigned long long int* ack, char* buf) {
    *ack = 0;
    for (int i = 1; i <= 8; i++) 
        (*ack) = ((*ack) << 8) | (unsigned long long int)(unsigned char)buf[i];
}

/*
 * Based on estimated RTT, adjust time_out interval
 * IMPORTANT: Do NOT get new sample_RTT for every packet!
 */
void time_outUpdate(int sample_RTT){
    double alpha = 0.125, beta = 0.25;   // const for estimating RTT

    estimate_RTT = (1 - alpha) * estimate_RTT + alpha * sample_RTT;
    dev_RTT = (1 - beta) * dev_RTT + beta * abs(sample_RTT - estimate_RTT);
    #if DEBUG
    printf("sRTT: %d, eRTT: %d, dRTT: %d\n", sample_RTT, estimate_RTT, dev_RTT);
    #endif
    time_out.tv_sec = (estimate_RTT + dev_RTT * 4) / 1000;
    time_out.tv_usec = ((estimate_RTT + dev_RTT * 4) % 1000) * 1000;
}

/* 
 * Three-way handshake
 */

void tcpConnect() {

    gettimeofday(&base_time, NULL);     // set tcp base time

    unsigned long long int seq = 0;

    char header[HEADER_SIZE + 1], connect_buf[MSS + 1];
    bzero(connect_buf, sizeof(connect_buf));
    bzero(header, sizeof(header));

    packHeader(SYN1, seq, 0, header);

    // first handshake
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");
    
    int t1 = clockTick();

    // set time out to 50 ms
    time_out.tv_sec = 0;
    time_out.tv_usec = 50 * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));  // set timeout

    while (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) {
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
        t1 = clockTick();
    }

    int t2 = clockTick();
    int sample_RTT = t2 - t1;
    time_outUpdate(sample_RTT);

    unsigned long long int ack = 0;

    unpackHeader(&ack, connect_buf);
    if (++seq != ack)  diep("connection set up is wrong");

    packHeader(SYN0, seq, 0, header);

    // third handshake (will not retransmitted)
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    puts("TCP connection is set up! Ready to send!");
}

/*
 * Transmit new packets after changing cwnd
 */
void transPkts(FILE* fd, unsigned long long int bytesToTransfer, char* header) {

    unsigned long long int last_seq = send_base + 1ULL * cwnd * MSS;
    //#if DEBUG 
    //printf("state = %d, cwnd = %d\n", state, cwnd);
    //printf("next = %llu, last = %llu\n", next_seq, last_seq);
    //#endif
    while (next_seq < last_seq) {
        int numbytes = fread(buffer+offset(next_seq)+HEADER_SIZE, sizeof(char), FILE_SIZE, fd);  
        if (numbytes < 0)  diep("file read");
        numbytes = min(numbytes, bytesToTransfer - bytes_tranferred);
        if (numbytes == 0)  break;     // no bytes to send
        #if DEBUG 
        printf("num = %d, bytes_transed = %llu\n", numbytes, bytes_tranferred);
        #endif
        packHeader(SYN0, next_seq, numbytes, header);
        memcpy(buffer+offset(next_seq), header, sizeof(char) * HEADER_SIZE);
        bytes_tranferred += numbytes;

        if (sendto(sockfd, buffer+offset(next_seq), MSS, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");

        time_stamp[offset(next_seq)] = clockTick();
        next_seq += 1ULL * MSS;
    }
}

/*
 * TCP packets sending process
 */

void tcpSend(FILE* fd, unsigned long long int bytesToTransfer) {

    puts("Start sending ...");
    int CA_cnt = 0;  // CONGEST_AVOID state cwnd changing helper
    // 1. send the first packet
    cwnd = 1;
    ssthresh = 64;
    dupACK = 0;
    state = SLOW_START;

    char ack_buf[HEADER_SIZE + 1], header[HEADER_SIZE + 1];
    bzero(ack_buf, sizeof(ack_buf));
    bzero(header, sizeof(header));
    
    state = SLOW_START;
    transPkts(fd, bytesToTransfer, header);

    // 2. start the state machine
    while (send_base != next_seq) {
        // 2.1 time out
        int deltaT = clockTick() - time_stamp[offset(send_base)];
        int rest_time = max(time_out.tv_sec * 1000 + time_out.tv_usec / 1000 - deltaT, 1);   // cannot be zero
        time_rest.tv_sec = rest_time / 1000;
        time_rest.tv_usec = rest_time * 1000;
        #if DEBUG
        int t1 = clockTick();
        #endif
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_rest, sizeof(time_rest));  // set timeout
        if (recvfrom(sockfd, ack_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) {
            #if DEBUG
            printf("time_rest: %d, time used: %d\n", rest_time, clockTick() - t1);
            printf("time_out: %d\n", (int)(time_out.tv_sec * 1000 + time_out.tv_usec / 1000));
            printf("RTT = %d\n", estimate_RTT);
            #endif
            ssthresh = cwnd / 2;    // BEAWARE OF HERE IN CASE SSTHRESH = 0
            cwnd = 1;
            dupACK = 0;
            if (sendto(sockfd, buffer+offset(send_base), MSS, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
                diep("sendto");
            time_stamp[offset(send_base)] = clockTick();

            // double time_out
            time_out.tv_sec = time_out.tv_sec * 2 + time_out.tv_usec * 2 / 1000000;
            time_out.tv_usec = time_out.tv_usec * 2 % 1000000;
            
            state = SLOW_START;
        } else {
            int sample_RTT = clockTick() - time_stamp[offset(send_base)];
            time_outUpdate(sample_RTT);

            unsigned long long int ack;
            unpackHeader(&ack, ack_buf);

            if (state == SLOW_START) {
                CA_cnt = 0;
                // 2.2 new ack
                if (ack > send_base) {
                    cwnd += (ack - send_base) / MSS;
                    send_base = ack;
                    dupACK = 0;
                    state = SLOW_START;
                    transPkts(fd, bytesToTransfer, header);
                // 2.3 duplicate ACK
                } else {
                    dupACK ++;
                    state = SLOW_START;
                    if (dupACK == 3) {  // fast retransmit
                        ssthresh = cwnd / 2;
                        cwnd = ssthresh + 3;
                        if (sendto(sockfd, buffer+offset(send_base), MSS, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
                            diep("sendto");
                        time_stamp[offset(send_base)] = clockTick();
                        transPkts(fd, bytesToTransfer, header);
                        state = FAST_RECOVERY;
                    }
                }
                if (state == SLOW_START && cwnd >= ssthresh)  state = CONGEST_AVOID;
            } else if (state == CONGEST_AVOID) {
                // 2.2 new ack
                if (ack > send_base) {
                    CA_cnt += (ack - send_base) / MSS;
                    send_base = ack;
                    if (CA_cnt >= cwnd){  
                        cwnd ++;
                        CA_cnt = 0;     // THIS IS IMPORTANT
                    }
                    dupACK = 0;
                    state = CONGEST_AVOID;
                    transPkts(fd, bytesToTransfer, header);
                // 2.3 duplicate ACK
                } else {
                    dupACK ++;
                    state = CONGEST_AVOID;
                    if (dupACK == 3) {  // fast retransmit
                        ssthresh = cwnd / 2;
                        cwnd = ssthresh + 3;
                        if (sendto(sockfd, buffer+offset(send_base), MSS, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
                            diep("sendto");
                        time_stamp[offset(send_base)] = clockTick();
                        transPkts(fd, bytesToTransfer, header);
                        state = FAST_RECOVERY;
                    }
                }

            } else if (state == FAST_RECOVERY){
                CA_cnt = 0;
                // 2.2 new ack
                if (ack > send_base) {
                    send_base = ack;
                    cwnd = max(ssthresh, 1);
                    dupACK = 0;
                    transPkts(fd, bytesToTransfer, header);
                    state = CONGEST_AVOID;
                // 2.3 duplicate ACK
                } else {
                    cwnd ++;
                    transPkts(fd, bytesToTransfer, header);
                    state = FAST_RECOVERY;
                }
            }
        }
    }
}

/*
 * TCP close, 4 time hand-waves
 */
void tcpDisconnect() {

    char header[HEADER_SIZE + 1], connect_buf[HEADER_SIZE + 1];
    bzero(connect_buf, sizeof(connect_buf));
    bzero(header, sizeof(header));

    packHeader(FIN, 0, 0, header);

    // first wave
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));  // set timeout
    while (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0) {
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
    }

    time_out.tv_sec = 0;
    time_out.tv_usec = 0;    // cancel the time out mechanic
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));

    if (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) < 0)
        diep("recv FIN from server");

    packHeader(FIN_ACK, 0, 0, header);
    if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
        diep("sendto");

    // waiting for 50 ms in case the last ACK is lost
    time_out.tv_sec = 0;
    time_out.tv_usec = 50 * 1000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&time_out, sizeof(time_out));  // set timeout
    while (recvfrom(sockfd, connect_buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t*)&slen) >= 0) {  // recv FIN again
        if (sendto(sockfd, header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, (socklen_t)slen) < 0)
            diep("sendto");
    }

    // after 50 ms, the client will close
    puts("TCP connection tears down! (client side)");
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    //Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

	/* Send data and receive acknowledgements on sockfd*/

    tcpConnect();
    tcpSend(fp, bytesToTransfer);
    tcpDisconnect();

    printf("Closing the socket\n");
    close(sockfd);
    fclose(fp);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);


    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}


