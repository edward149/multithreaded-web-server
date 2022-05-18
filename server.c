#define _POSIX_C_SOURCE 200112L
#define REQTYPE 0
#define REQPATH 1
#define REQPHTTP 2
#define SPLITHEADERS 3
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

void read_input(int* protocolPos, int* portPos, int* pathPos, int argc, char** argv);

int main(int argc, char** argv) {
    int protocolPos, portPos, pathPos;

    read_input(&protocolPos, &portPos, &pathPos, argc, argv);

	int sockfd, newsockfd, n, re, s;
	char buffer[256];
	struct addrinfo hints, *res;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

	if (argc < 4) {
		fprintf(stderr, "ERROR, BAD INPUT\n");
		exit(EXIT_FAILURE);
	}

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;       // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, argv[portPos], &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);

	// Listen on socket - means we're ready to accept connections,
	// incoming connection requests will be queued, man 3 listen
	if (listen(sockfd, 5) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	client_addr_size = sizeof client_addr;
	newsockfd =
		accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
	if (newsockfd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}

	// Read characters from the connection, then process
	n = read(newsockfd, buffer, 255); // n is number of characters read
	if (n < 0) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	// Null-terminate string
	buffer[n] = '\0';
	printf("Here is the message: %s\n", buffer);

	char *token, *theRest;
	char *headerSplit[SPLITHEADERS];
	int headerParts = 0;
	theRest = buffer;

	while ((token = strtok_r(theRest, " ", &theRest)) && headerParts != 3) {
		headerSplit[headerParts] = token;
		headerParts++;
	}

	if (strcmp(headerSplit[0], "GET1") || strcmp(headerSplit[0], "POST")) {
		printf("%s returns 404\n", headerSplit[REQPATH]);
	}
	// Write message back
	n = write(newsockfd, "I got your message", 18);
	if (n < 0) {
		perror("write");
		exit(EXIT_FAILURE);
	}

	close(sockfd);
	close(newsockfd);
	return 0;
}

void read_input(int* protocolPos, int* portPos, int* pathPos, int argc, char** argv) {
    int portFlag;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "4") == 0 || strcmp(argv[i], "6") == 0) {
            *protocolPos = i;
        }
        if (strlen(argv[i]) != 1) {
            portFlag = 1;
            for (int j = 0; j < strlen(argv[i]); j++) {
                if (!isdigit(argv[i][j])) {
                    portFlag = 0;
                }
            }
            if (portFlag == 1) {
                *portPos = i;
            } else {
                *pathPos = i;
            }
        }
    }
}
