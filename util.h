
#define EVENT_OWNER_CLIENT 1
#define EVENT_OWNER_WORKER 2

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
    char resource_url[100];
}request_item;

int parse_port_number(int argc, char* argv);
int increase_fd_limit(int max_fd_limit);
int make_socket_non_blocking(int fd);
int create_worker_threads(int no_threads, void (*func)(void*));
request_item* create_request_item(int type, char* name);
