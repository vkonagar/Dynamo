all: csapp.c server.c
	gcc -g csapp.c server.c http.c dynlib_cache.c http_header.c util.c -lpthread -ldl -o server
server_unopt: csapp.c server_unopt.c
	gcc -g csapp.c server_unopt.c dynlib_cache.c util.c http.c http_header.c -lpthread -ldl -o server_unopt
clean:
	rm -f server *.o a.out
