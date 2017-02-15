#include "http.h"

#ifndef UTIL_H
#define UTIL_H

#define EVENT_OWNER_CLIENT  1
#define EVENT_OWNER_WORKER  2

#define MAX_RESOURCE_NAME_LENGTH    100
#define WORKER_THREAD_PORT  9898

#define MAX_PATH_CHARS      10
#define CGIBIN_DIR_NAME     "cgi-bin"
#define STATIC_DIR_NAME     "static"
#define MAX_READ_LENGTH     4096

#define MAX_DLL_NAME_LENGTH 10
#define MAX_DLL_PATH_LENGTH 10

#define RESPONSE_HANDLING_COMPLETE  1
#define RESPONSE_HANDLING_PARTIAL   2

#define SHARED_SOCKET       1


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
    char resource_name[MAX_RESOURCE_NAME_LENGTH];
    int client_fd; /* Required to perform sendfile directly for STATIC request type*/
}request_item;

int parse_port_number(int argc, char* argv);
int increase_fd_limit(int max_fd_limit);
int make_socket_non_blocking(int fd);
int create_worker_threads(int no_threads, void (*func)(void*));
request_item* create_dynamic_request_item(char* name);
request_item* create_static_request_item(char* name, int client_fd);
void add_worker_fd_to_epoll(int epollfd, int worker_fd, int cli_fd);
void add_client_fd_to_epoll(int epollfd, int cli_fd);
int send_to_worker_thread(request_item* reqitem);
void handle_static(int fd, char* resource_name);
void handle_unknown(int fd, char* resource_name);
void create_static_worker(int client_fd, void (*func)(void*), char* res_name);
#endif
