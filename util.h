#ifndef __UTIL_H  /* Guard macros */
#define __UTIL_H

#include "http_util.h"
#include <pthread.h>

#define STAT_INTERVAL               5 /* Display interval for statistics */

#define EVENT_OWNER_CLIENT          1
#define EVENT_OWNER_WORKER          2

#define MAX_RESOURCE_NAME_LENGTH    100
#define WORKER_THREAD_PORT          9898 /* All of the worker threads listen
                                            on this port. Kernel dynamically
                                            load balances master's request
                                            between worker threads */

#define MAX_PATH_CHARS              10 /* Path name characters
                                          ex /cgi-bin/cmu.jpg has 2 path chars
                                        */
#define CGIBIN_DIR_NAME             "cgi-bin"
#define STATIC_DIR_NAME             "static"
#define MAX_READ_LENGTH             4096 /* Max read size in single IO request*/

/* Path name size of dynamic request urls */
#define MAX_DLL_NAME_LENGTH         20
#define MAX_DLL_PATH_LENGTH         20

#define RESPONSE_HANDLING_COMPLETE  1
#define RESPONSE_HANDLING_PARTIAL   2

/* Indicates if a socket is shared between multiple threads */
#define SHARED_SOCKET               1
#define NON_SHARED_SOCKET           2

#define SERVER_REQUIRED_CMD_ARG_COUNT 2

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

typedef struct epoll_conn_state
{
    int type; /* Who is owner of this state */
    int client_fd;
    int worker_fd; /* Assigned workerfd for the client's fd */
    struct epoll_conn_state* client_con;
    /* client_con is required because when workerfd is done, master gets a
     * notification. Master then takes the output from worker fd and writes
     * it to the client's fd and closes it.
     * Now, we also need to free client's epoll_conn_state which was added earlier.
     * To do the same, we need to have a pointer to clientfd's epoll_con_state
     * in the workerfd's epoll_con_state */
}epoll_conn_state;

/* Structure to pass information between master and worker threads */
typedef struct request_item
{
    char resource_name[MAX_RESOURCE_NAME_LENGTH];
    int client_fd; /* Required to perform sendfile directly for STATIC request type*/
}request_item;

void increment_reply_count();
long get_reply_count();
int parse_port_number(int argc, char* argv);
int increase_fd_limit(int max_fd_limit);
int make_socket_non_blocking(int fd);
int create_threads(int no_threads, void* (*func)(void*));
request_item* create_dynamic_request_item(char* name);
request_item* create_static_request_item(char* name, int client_fd);
void add_worker_fd_to_epoll(int epollfd, int worker_fd, epoll_conn_state*);
void add_client_fd_to_epoll(int epollfd, int cli_fd);
int send_to_worker_thread(request_item* reqitem);
void handle_static(int fd, char* resource_name);
void handle_unknown(int fd, char* resource_name);
void create_static_worker(int client_fd, void* (*func)(void*), char* res_name);
int create_listen_tcp_socket(int port, int backlog, int socket_shared);
void handle_dynamic_exec_lib(int client_fd, char* resource_name);
void init_stat_mutexes();
void increment_request_count();
long get_request_count();
void create_stat_thread();
#endif
