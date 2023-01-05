/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 1024 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes, cnt;  
	char buf[MAXDATASIZE];
	char *p_arg, *ip_seq = NULL, *port_seq = NULL, *file_seq = NULL;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
	int arglen, flag = 0;
	FILE *fptr = NULL;

	if (argc != 2) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	arglen = strlen(argv[1]);
	if (arglen < 7 || strncmp(argv[1], "http://", 7) != 0) {
		fprintf(stderr,"client: use the http format\n");
	    exit(1);
	}
	
	for (cnt = 0, p_arg = argv[1] + 7; (*p_arg) != '\0'; cnt++, p_arg++) {
		if ((*p_arg) == ':')  break;
		if ((*p_arg) == '/')  break;
	}
	ip_seq = malloc((cnt + 1) * sizeof(char));
	memcpy(ip_seq, p_arg-cnt, cnt * sizeof(char));
	ip_seq[cnt] = '\0';

	if ((*p_arg) == ':') {
		p_arg ++;
		for (cnt = 0; (*p_arg) != '\0' && (*p_arg) != '/'; cnt++, p_arg++);
		port_seq = malloc((cnt + 1) * sizeof(char));
		memcpy(port_seq, p_arg-cnt, cnt * sizeof(char));
		port_seq[cnt] = '\0';
	} else {
		port_seq = malloc(3 * sizeof(char));
		memcpy(port_seq, "80", 2 * sizeof(char));
		port_seq[2] = '\0';
	}
	if ((*p_arg) == '\0') {
		free(ip_seq);
		free(port_seq);
		fprintf(stderr,"client: give the file name\n");
	    exit(1);
	}

	for (cnt = 0; (*p_arg) != '\0'; cnt++, p_arg++);
	file_seq = malloc((strlen(argv[1]) + 19) * sizeof(char));
	memcpy(file_seq, "GET ", 4 * sizeof(char));
	memcpy(file_seq+4, p_arg-cnt, cnt * sizeof(char));
	memcpy(file_seq+cnt+4, " HTTP/1.1\r\n", 11 * sizeof(char));
	memcpy(file_seq+cnt+15, "Host: ", 6 * sizeof(char));
	memcpy(file_seq+cnt+21, argv[1]+7, (strlen(argv[1])-7-cnt) * sizeof(char));
	memcpy(file_seq+strlen(argv[1])+14, "\r\n\r\n", 4 * sizeof(char));
	file_seq[strlen(argv[1]) + 18] = '\0';

	/*puts(ip_seq);
	puts(port_seq);
	puts(file_seq);*/

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((rv = getaddrinfo(ip_seq, port_seq, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	free(ip_seq);
	free(port_seq);
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		free(file_seq);
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	numbytes = send(sockfd, file_seq, strlen(file_seq), 0);
	if (numbytes != strlen(file_seq)) {
		free(file_seq);
		perror("send");
		exit(1);
	}
	free(file_seq);
	memset(buf, 0, sizeof(buf));

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	printf("client: received ");

	if ((fptr = fopen("output", "w")) == NULL) {
		fprintf(stderr, "client: failed to open OUTPUT\n");
		return 3;
	}
	
	do {
		printf("%s", buf);
		if (flag == 0) {
			for (cnt = 0; buf[cnt] != '\0'; cnt++) {
				if (cnt >= 3 && buf[cnt-3] == '\r' && buf[cnt-2] == '\n' 
							 && buf[cnt-1] == '\r' && buf[cnt] == '\n') {
					flag = 1;
					if (fwrite(buf+cnt+1, sizeof(char), numbytes-cnt-1, fptr) < numbytes-cnt-1) {
						fprintf(stderr, "client: failed to write into OUTPUT\n");
						return 4;
					}
					break;
				} 
			}
		} else {
			if (fwrite(buf, sizeof(char), numbytes, fptr) < numbytes) {
				fprintf(stderr, "client: failed to write into OUTPUT\n");
				return 4;
			}
		}

		memset(buf, 0, sizeof(buf));
		
	} while ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0);
	fclose(fptr);
	close(sockfd);

	return 0;
}

