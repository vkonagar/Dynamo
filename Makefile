all: csapp.c server.c dynlib.c http_header.c util.c http_util.c
	gcc -g csapp.c server.c http_util.c dynlib.c http_header.c util.c -lpthread -ldl -o server
# Make unoptimzed server
server_unopt: csapp.c server_unopt.c dynlib.c util.c http_util.c http_header.c
	gcc -g csapp.c server_unopt.c dynlib.c util.c http_util.c http_header.c -lpthread -ldl -o server_unopt
clean:
	rm -f server *.o a.out server_unopt
