all: csapp.c server.c http_header.c util.c http_util.c cache.c
	gcc -DCACHING_ENABLED -g csapp.c server.c http_util.c http_header.c util.c cache.c -lpthread -ldl -o server
# Make unoptimzed server
server_unopt: csapp.c server_unopt.c util.c http_util.c http_header.c
	gcc -g csapp.c server_unopt.c util.c http_util.c http_header.c -lpthread -ldl -o server_unopt
clean:
	rm -f server *.o a.out server_unopt
