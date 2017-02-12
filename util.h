#include "http.h"

#ifndef UTIL_H
#define UTIL_H

#define EVENT_OWNER_CLIENT 1
#define EVENT_OWNER_WORKER 2

#define WORKER_THREAD_PORT      9898

#define DEBUG

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

typedef struct epoll_conn_state
{
    int type; /* Who is handling this state */
    int client_fd;
    int worker_fd;
}epoll_conn_state;

typedef struct request_item
{
#define REQUEST_TYPE_DYNAMIC_CONTENT 1
#define REQUEST_TYPE_STATIC_CONTENT 2
    int request_type;
    char resource_url[MAX_URL_LENGTH];
}request_item;

int parse_port_number(int argc, char* argv);
int increase_fd_limit(int max_fd_limit);
int make_socket_non_blocking(int fd);
int create_worker_threads(int no_threads, void (*func)(void*));
request_item* create_request_item(int type, char* name);
void add_worker_fd_to_epoll(int epollfd, int worker_fd, int cli_fd);
void add_client_fd_to_epoll(int epollfd, int cli_fd);
int send_to_worker_thread(request_item* reqitem);
#endif
