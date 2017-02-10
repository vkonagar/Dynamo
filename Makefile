all: csapp.c server.c
	gcc csapp.c server.c http.c http_header.c -lpthread -o server
server_opt: csapp.c server_optimized.c
	gcc csapp.c server_optimized.c http.c http_header.c -lpthread -o server
clean:
	rm -f server *.o a.out
