#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <limits.h>
#include <sys/resource.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include "util.h"
#include "dynlib_cache.h"
#include "dlfcn.h"

void create_static_worker(int client_fd, void (*func)(void*), char* res_name)
{
    pthread_t thread_id;
    request_item* item = create_static_request_item(res_name, client_fd);
    pthread_create(&thread_id, NULL, func, (void*) item);
}

void handle_dynamic_exec_lib(int client_fd, char* resource_name)
{
    int path_len = MAX_DLL_NAME_LENGTH + strlen(CGIBIN_DIR_NAME) + MAX_PATH_CHARS;
    char lib_path[path_len];
    snprintf(lib_path, path_len, "./%s/%s.so", CGIBIN_DIR_NAME, resource_name);
    void* handle = load_dyn_library(lib_path);
    if (handle == NULL)
    {
        http_write_response_header(client_fd, HTTP_404);
    }
    else
    {
        /* Success */
        void (*func)(int) = dlsym(handle, "cgi_function");
        func(client_fd);
        unload_dyn_library(handle);
    }
}

void handle_static(int fd, char* resource_name)
{
    int path_len = MAX_RESOURCE_NAME_LENGTH + strlen(STATIC_DIR_NAME) + MAX_PATH_CHARS;
    char res_path[path_len];
    snprintf(res_path, path_len, "./%s/%s", STATIC_DIR_NAME, resource_name);
    /* Now read and write the resource */
    int filefd = open(res_path, O_RDONLY);
    if (filefd == -1)
    {
        perror("open");
        http_write_response_header(fd, HTTP_404);
        return;
    }
    http_write_response_header(fd, HTTP_200);
    int read_count;
    while ((read_count = sendfile(fd, filefd, 0, MAX_READ_LENGTH)) > 0);
    if (read_count == -1)
    {
        perror("sendfile");
    }
    close(filefd);
}

void handle_unknown(int fd, char* resource_name)
{
    printf("Unknown resource type\n");
}

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

request_item* create_dynamic_request_item(char* name)
{
    request_item* item = malloc(sizeof(request_item));
    sprintf(item->resource_name, "%s", name);
    return item;
}

request_item* create_static_request_item(char* name, int client_fd)
{
    request_item* item = malloc(sizeof(request_item));
    sprintf(item->resource_name, "%s", name);
    item->client_fd = client_fd;
    return item;
}

void add_client_fd_to_epoll(int epollfd, int cli_fd)
{
    struct epoll_event event;
    epoll_conn_state* conn = malloc(sizeof(epoll_conn_state));
    conn->client_fd = cli_fd;
    conn->worker_fd = -1;
    conn->type = EVENT_OWNER_CLIENT;

    event.data.ptr = conn;
    event.events = EPOLLIN | EPOLLET | EPOLLHUP | EPOLLERR;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cli_fd, &event) == -1)
    {
        perror("epoll add client fd");
        exit(EXIT_FAILURE);
    }
}


void add_worker_fd_to_epoll(int epollfd, int worker_fd, int cli_fd)
{
    struct epoll_event event;
    epoll_conn_state* conn = malloc(sizeof(epoll_conn_state));
    conn->client_fd = cli_fd;
    conn->worker_fd = worker_fd;
    conn->type = EVENT_OWNER_WORKER;

    event.data.ptr = conn;
    event.events = EPOLLIN | EPOLLHUP | EPOLLERR; /* Level triggered */

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, worker_fd, &event) == -1)
    {
        perror("epoll add client fd");
        exit(EXIT_FAILURE);
    }
}

int send_to_worker_thread(request_item* reqitem)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(WORKER_THREAD_PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)
    {
        dbg_printf("inet_pton error occured\n");
        return -1;
    }
    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("Connect to worker thread");
    }
    rio_writen(sockfd, reqitem, sizeof(request_item));
    return sockfd;
}
