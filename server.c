#define _POSIX_C_SOURCE 200112L
#define REQPATH 1
#define FOUND "HTTP/1.0 200 OK\r\n"
#define NOT_FOUND "HTTP/1.0 404 NOT FOUND\r\n\r\n"
#define HTML "Content-Type: text/html\r\n\r\n"
#define JPEG "Content-Type: image/JPEG\r\n\r\n"
#define CSS "Content-Type: text/css\r\n\r\n"
#define JAVASCRIPT "Content-Type: text/javascript\r\n\r\n"
#define OTHER_TYPE "Content-Type: application/octet-stream\r\n\r\n"
#define SPLITHEADERS 3

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

struct stat st;

void read_input(int *protocolPos, int *portPos, int *pathPos, int argc, char **argv);

int main(int argc, char** argv) {
    int protocolPos, portPos, pathPos;
	protocolPos = portPos = pathPos = 0;

    read_input(&protocolPos, &portPos, &pathPos, argc, argv);

	int sockfd, newsockfd, n, re, s;
	char buffer[2001];
	struct addrinfo hints, *res;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;

	if (argc != 4) {
		fprintf(stderr, "ERROR, BAD INPUT\n");
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[protocolPos], "4") != 0) {
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

		// Read characters from the connection, then process
		n = read(newsockfd, buffer, 255); // n is number of characters read
		if (n < 0) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		// Null-terminate string
		buffer[n] = '\0';

		//splits req header into 3 and stores them for use later
		char *token, *theRest, *fileType;
		char *headerSplit[SPLITHEADERS];
		int headerParts = 0;
		theRest = buffer;
		while ((token = strtok_r(theRest, " ", &theRest)) && headerParts != 3) {
			headerSplit[headerParts] = token;
			headerParts++;
		}

		//stores path to requested file and gets file type
		char *pathDupe = strdup(headerSplit[REQPATH]);
		char *webRootFile = strdup(argv[pathPos]);
		theRest = pathDupe;
		char fileToOpen[strlen(pathDupe) + strlen(webRootFile) + 1];
		strcpy(fileToOpen, webRootFile);
		strcat(fileToOpen, pathDupe);
		printf("%s\n", fileToOpen);
		while ((token = strtok_r(theRest, ".", &theRest))) {
			fileType = token;
		}
		struct stat st;
		stat(fileToOpen, &st);

		//Checks if file requested is a file, then write message back
		if (S_ISREG(st.st_mode) != 0) {
			if (open(fileToOpen, O_RDONLY) != -1) {
				n = write(newsockfd, FOUND, strlen(FOUND));
				if (strcmp(fileType, "html") == 0)
					n = write(newsockfd, HTML, strlen(HTML));
				else if (strcmp(fileType, "jpg") == 0)
					n = write(newsockfd, JPEG, strlen(JPEG));
				else if (strcmp(fileType, "css") == 0)
					n = write(newsockfd, CSS, strlen(CSS));
				else if (strcmp(fileType, "js") == 0)
					n = write(newsockfd, JAVASCRIPT, strlen(JAVASCRIPT));
				else
					n = write(newsockfd, OTHER_TYPE, strlen(OTHER_TYPE));

				n = sendfile(newsockfd, open(fileToOpen, O_RDONLY), NULL, st.st_size);
				if (n < 0) {
					perror("write");
					exit(EXIT_FAILURE);
				}
			} else {
				n = write(newsockfd, NOT_FOUND, strlen(NOT_FOUND));
			}
		} else {
			n = write(newsockfd, NOT_FOUND, strlen(NOT_FOUND));
		}
		free(pathDupe);
		close(newsockfd);		
	}
	close(sockfd);
	return 0;
}

//reads input and stores index positions of the protocol number, port number and path
void read_input(int *protocolPos, int *portPos, int *pathPos, int argc, char **argv) {
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
