server: server.c
	gcc -Wall -pthread -o server server.c

clean:
	rm server