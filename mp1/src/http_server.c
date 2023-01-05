/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to
#define HEADER_SIZE 1024
#define FILE_SIZE 1024

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

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
	if (argc != 2) {
	    fprintf(stderr, "port number not specified\n");
	    exit(1);
	}
	FILE *fptr = NULL;
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN], header[HEADER_SIZE], file[FILE_SIZE];
	char *file_name = NULL;
	int rv, cnt;
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// puts(argv[1]);
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		// their_addr has the client's IP addr
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		// enable concurrency
		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
			if (recv(new_fd, header, HEADER_SIZE-1, 0) == -1)
				perror("recv");
			
			if (strlen(header) < 4 || strncmp(header, "GET ", 4) != 0) {
				if (send(new_fd, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0) == -1)
						perror("send");
			} else {
				if ((header[4]) != '/') {
					if (send(new_fd, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0) == -1)
						perror("send");
				} else {
					file_name = &header[4];
					for (cnt = 0; file_name[cnt] != '\0'; cnt++) {
						if (file_name[cnt] == ' ' || file_name[cnt] == '\n')  break;
						if (file_name[cnt] == '\r')  break;
					}
					file_name += cnt;
					if (strlen(file_name) < 9 || strncmp(file_name, " HTTP/1.1", 9) != 0) {
						if (send(new_fd, "HTTP/1.1 400 Bad Request\r\n\r\n", 28, 0) == -1) 
							perror("send");
						file_name = NULL;
					} else {
						file_name = &header[4];
						file_name[cnt] = '\0';
					}
				}
				
			}
			if (file_name == NULL);
			else if ((fptr = fopen(file_name+1, "r")) == NULL) {
				if (send(new_fd, "HTTP/1.1 404 Not Found\r\n\r\n", 26, 0) == -1)
					perror("send");
			} else {
				memset(file, 0, sizeof(file));
				memcpy(file, "HTTP/1.1 200 OK\r\n\r\n", 19);
				fread(file+19, sizeof(char), FILE_SIZE-20, fptr);
				//puts(file_name);
				// send all the file content
				do {
					//puts(file);
					//printf("%ld\n", strlen(file));
					if (send(new_fd, file, strlen(file), 0) == -1)
						perror("send");
					memset(file, 0, sizeof(file));
				} while (fread(file, sizeof(char), FILE_SIZE-1, fptr) > 0);
			}
			fclose(fptr);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

