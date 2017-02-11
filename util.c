#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <limits.h>
#include <sys/resource.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include "util.h"

int make_socket_non_blocking (int sfd)
{
    int flags, s;
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }
    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl");
        return -1;
    }
    return 0;
}

int increase_fd_limit(int max_fds)
{
    /* Increase the max fd resource limit */
    struct rlimit res;
    res.rlim_cur = max_fds;
    res.rlim_max = max_fds;
    if(setrlimit(RLIMIT_NOFILE, &res) == -1)
    {
	    perror("Resource FD limit");
        return -1;
    }
    return 1;
}

int parse_port_number(int argc, char* argv)
{
    if (argc == 2)
    {
        errno = 0;
        int port = strtol(argv, NULL, 10);
        if (((port == LONG_MIN || port == LONG_MAX) && errno == ERANGE) || port == 0)
        {
            printf("Provide a valid port number\n");
            exit(EXIT_FAILURE);
        }
        return port;
    }
    return -1;
}

int create_listen_tcp_socket(int port, int backlog, int socket_shared)
{
    int sfd = Socket(AF_INET, SOCK_STREAM, 0);
    if (socket_shared)
    {
        int optval = 1;
        setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    }
    int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    /* Bind the socket to a port */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    Bind(sfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    Listen(sfd, backlog);
    return sfd;
}

int create_worker_threads(int no_threads, void (*func)(void*))
{
    int i;
    for(i=0; i<no_threads; i++)
    {
        pthread_t thread_id;
        int* id = malloc(sizeof(int));
        *id = i;
        pthread_create(&thread_id, NULL, func, (void*) id);
    }
}

request_item* create_request_item(int type, char* url)
{
    request_item* item = malloc(sizeof(request_item));
    item->request_type = type;
    strcpy(item->resource_url, url);
    return item;
}
