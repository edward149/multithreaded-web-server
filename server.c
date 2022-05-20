#define _POSIX_C_SOURCE 200809L
#define REQPATH 1
#define FOUND "HTTP/1.0 200 OK\r\n"
#define NOT_FOUND "HTTP/1.0 404 NOT FOUND\r\n"
#define HTML "Content-Type: text/html\r\n\r\n"
#define JPEG "Content-Type: image/JPEG\r\n\r\n"
#define CSS "Content-Type: text/css\r\n\r\n"
#define JAVASCRIPT "Content-Type: text/javascript\r\n\r\n"
#define OTHER_TYPE "Content-Type: application/octet-stream\r\n\r\n"
#define SPLITHEADERS 3
#define MULTITHREADED
#define IMPLEMENTS_IPV6

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

struct stat st;
struct data {
	int n;
	char buffer[2001];
	int newsockfd;
	char *webRoot;
};

void read_input(int *protocolPos, int *portPos, int *webRootPos, int argc, char **argv);
void check_ipv(char **argv, int protocolPos);
void *handle_connection(void *clientDataPtr);

int main(int argc, char** argv) {
	// lines 39 to 118 are taken from practical 8 and changed for my use,
	// written by the University of Melbourne COMP30023 teaching staff
    int protocolPos, portPos, webRootPos;
	protocolPos = portPos = webRootPos = 0;
	int sockfd, newsockfd, re, s;
	int n = 0;
	struct addrinfo hints, *res, *rp;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

	read_input(&protocolPos, &portPos, &webRootPos, argc, argv);
    check_ipv(argv, protocolPos);

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	if (strcmp(argv[protocolPos], "4") == 0)
		hints.ai_family = AF_INET;       // IPv4
	else if (strcmp(argv[protocolPos], "6") == 0)
		hints.ai_family = AF_INET6;       // IPv6		
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, argv[portPos], &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	// Create socket
	if (hints.ai_family == AF_INET) {
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0) {
			perror("socket");
			exit(EXIT_FAILURE);
		}
	} else if (hints.ai_family == AF_INET6) {
		for (rp = res; rp!= NULL; rp = rp->ai_next) {
			if (rp->ai_family == AF_INET6 && (sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) < 0 ) {
				perror("socket");
				exit(EXIT_FAILURE);
			}
		}
	}

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
	perror("setsockopt");
	exit(1);
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

	while (1) {
		// Accept a connection - blocks until a connection is ready to be accepted
		// Get back a new file descriptor to communicate on
		client_addr_size = sizeof client_addr;
		newsockfd =
			accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
		if (newsockfd < 0) {
			perror("accept");
			exit(EXIT_FAILURE);
		}

		pthread_t thread;
		struct data clientData;
		clientData.n = n;
		clientData.newsockfd = newsockfd;
        if (strlen(argv[webRootPos]) != 0)
            strcpy(clientData.webRoot, argv[webRootPos]);
		struct data *psockfd = malloc(sizeof(struct data));
		*psockfd = clientData;
		pthread_create(&thread, NULL, handle_connection, (void *)psockfd);
	}
	close(sockfd);
	return 0;
}

//reads input and stores index positions of the protocol number, port number and path
void read_input(int *protocolPos, int *portPos, int *webRootPos, int argc, char **argv) {
    int portFlag;

    if (argc != 4) {
		fprintf(stderr, "ERROR, BAD INPUT\n");
		exit(EXIT_FAILURE);
	}

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
                *webRootPos = i;
            }
        }
    }
}

//checks whether input is valid, only accepts 4 and 6
void check_ipv(char **argv, int protocolPos) {
    if (strcmp(argv[protocolPos], "4") != 0 && strcmp(argv[protocolPos], "6") != 0) {
		fprintf(stderr, "ERROR, BAD IPV\n");
		exit(EXIT_FAILURE);
	}
}

//function to read input from client side
//returns a http response
void *handle_connection(void *clientDataPtr) {
	struct data *my_newsockfd = (struct data*)clientDataPtr;
	// Read characters from the connection, then process
	my_newsockfd->n = read(my_newsockfd->newsockfd, my_newsockfd->buffer, 2000); // n is number of characters read
	if (my_newsockfd->n < 0) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	// Null-terminate string
	my_newsockfd->buffer[my_newsockfd->n] = '\0';

	//splits req header into 3 and stores them for use later
	char *token, *theRest, *fileType;
	char *headerSplit[SPLITHEADERS];
	int headerParts = 0;
	theRest = my_newsockfd->buffer;
	while ((token = strtok_r(theRest, " ", &theRest)) && headerParts != 3) {
		headerSplit[headerParts] = token;
		headerParts++;
	}
	//processes path
	char *pathDupeFiletype = strdup(headerSplit[REQPATH]);
	char *webRootDupe = strdup(my_newsockfd->webRoot);
	char fileToOpen[strlen(pathDupeFiletype) + strlen(webRootDupe) + 1];
	strcpy(fileToOpen, webRootDupe);
	strcat(fileToOpen, pathDupeFiletype);
	char *fileToOpenDupe = strdup(fileToOpen);
	//checks for any escapes in path
	theRest = fileToOpenDupe;
	int escapeFlag = 0;
	while ((token = strtok_r(theRest, "/", &theRest))) {
		if (strcmp(token, "..") == 0) {
			escapeFlag = 1;
		}
	}
	//checks for file type of requested file
	theRest = pathDupeFiletype;
	while ((token = strtok_r(theRest, ".", &theRest))) {
		fileType = token;
	}
	struct stat st;
	stat(fileToOpen, &st);

	//Checks if file requested is a file, then write message back
	if (escapeFlag == 0) {
		if (S_ISREG(st.st_mode) != 0) {
			if (open(fileToOpen, O_RDONLY) != -1) {
				my_newsockfd->n = write(my_newsockfd->newsockfd, FOUND, strlen(FOUND));
				if (strcmp(fileType, "html") == 0)
					my_newsockfd->n = write(my_newsockfd->newsockfd, HTML, strlen(HTML));
				else if (strcmp(fileType, "jpg") == 0)
					my_newsockfd->n = write(my_newsockfd->newsockfd, JPEG, strlen(JPEG));
				else if (strcmp(fileType, "css") == 0)
					my_newsockfd->n = write(my_newsockfd->newsockfd, CSS, strlen(CSS));
				else if (strcmp(fileType, "js") == 0)
					my_newsockfd->n = write(my_newsockfd->newsockfd, JAVASCRIPT, strlen(JAVASCRIPT));
				else
					my_newsockfd->n = write(my_newsockfd->newsockfd, OTHER_TYPE, strlen(OTHER_TYPE));
				//i used write above as its simpler and takes less time to code lol
				//i used sendfile here as it returns the amount of bytes sent when sending a response to the client
				my_newsockfd->n = sendfile(my_newsockfd->newsockfd, open(fileToOpen, O_RDONLY), NULL, st.st_size);
				if (my_newsockfd->n < 0) {
					perror("write");
					exit(EXIT_FAILURE);
				}
			} else {
				my_newsockfd->n = write(my_newsockfd->newsockfd, NOT_FOUND, strlen(NOT_FOUND));
			}
		} else {
			my_newsockfd->n = write(my_newsockfd->newsockfd, NOT_FOUND, strlen(NOT_FOUND));
		}
	} else {
		my_newsockfd->n = write(my_newsockfd->newsockfd, NOT_FOUND, strlen(NOT_FOUND));
	}
	free(fileToOpenDupe);
	free(pathDupeFiletype);
	free(webRootDupe);
    free(clientDataPtr);
	close(my_newsockfd->newsockfd);
	return NULL;
}