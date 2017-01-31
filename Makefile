all: csapp.c server.c
	gcc csapp.c server.c http.c http_header.c -lpthread -o server
clean:
	rm -f server *.o a.out
