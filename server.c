#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include "http_header.h"
#include "http.h"
#include "util.h"
#include "dynlib_cache.h"
#include <sys/epoll.h>
#include <dlfcn.h>

#define TRUE                    1

#define MAX_READ_LENGTH         4096
#define DEFAULT_LISTEN_PORT     80
#define MAX_LISTEN_QUEUE        10000
#define MAX_NAME_LENGTH         100
#define MAX_READ_LENGTH         4096
#define MAX_FD_LIMIT            100000
#define MAX_EPOLL_EVENTS        10000


#define WORKER_THREAD_PORT      9898
#define WORKER_THREAD_COUNT     4

#define EPOLL_TRIGGER_TYPE      EPOLLET


#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif


void add_client_worker_fd_to_epoll(int epollfd, int cli_fd, int worker_fd, int type)
{
    struct epoll_event event;
    epoll_conn_state* conn = malloc(sizeof(epoll_conn_state));
    conn->client_fd = cli_fd;
    conn->worker_fd = worker_fd;
    conn->type = type;

    event.data.ptr = conn;
    event.events = EPOLLIN | EPOLL_TRIGGER_TYPE;

    int fd_to_add = cli_fd;
    if (type == EVENT_OWNER_WORKER)
    {
        fd_to_add = worker_fd;
    }

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd_to_add, &event) == -1)
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

void handle_client_request(int epollfd, epoll_conn_state* con)
{
    http_header_t header;
    init_header(&header);
    http_scan_header(con->client_fd, &header);
    dbg_printf("YOOO: %s\n", header.request_url);
    request_item* reqitem = create_request_item(REQUEST_TYPE_DYNAMIC_CONTENT,
                                                        header.request_url);
    int worker_fd = send_to_worker_thread(reqitem);
    free(reqitem);

    con->worker_fd = worker_fd;

    add_client_worker_fd_to_epoll(epollfd, con->client_fd, worker_fd, EVENT_OWNER_WORKER);
    free_kvpairs_in_header(&header);
}

void handle_client_response(int epollfd, epoll_conn_state* con)
{
    char buf[MAX_READ_LENGTH];
    int read_count = 0;
    int written_count = 0;
    while ((read_count = read(con->worker_fd, buf + read_count, MAX_READ_LENGTH)) > 0)
    {
        dbg_printf("Read %d\n", read_count);
        int a = rio_writen(con->client_fd, buf + written_count, read_count);
        dbg_printf("Written %d\n", a);
        written_count += read_count;
    }
}

void worker_thread(void* arg)
{
    int thread_id = *(int*)arg;
    free(arg);
    int server_sock = create_listen_tcp_socket(WORKER_THREAD_PORT, MAX_LISTEN_QUEUE, TRUE);
    /* Sequential server */
    request_item item;
    while (1)
    {
        int client_fd = accept(server_sock, NULL, NULL);
        if (client_fd == -1)
            continue;

        /* Read the request from the master */
        read(client_fd, &item, sizeof(request_item));

        char resource_name[MAX_NAME_LENGTH];
        if (get_resource_type(item.resource_url, resource_name) == RESOURCE_TYPE_CGI_BIN)
        {
            char lib_path[MAX_DLL_NAME_LENGTH + MAX_DLL_PATH_LENGTH];
            sprintf(lib_path, "./cgi-bin/%s.so", resource_name);
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
        close(client_fd);
    }
}

int main(int argc, char *argv[])
{
    increase_fd_limit(MAX_FD_LIMIT);
    signal(SIGPIPE, SIG_IGN); /* Ignore Sigpipe */
    int port = parse_port_number(argc, argv[1]);
    if (port == -1)
    {
        fprintf(stderr, "Using default port number %d\n", DEFAULT_LISTEN_PORT);
        port = DEFAULT_LISTEN_PORT;
    }

    /* Create a new server socket */
    int server_sock = create_listen_tcp_socket(port, MAX_LISTEN_QUEUE);
    make_socket_non_blocking(server_sock);

    /* Create worker threads */
    create_worker_threads(WORKER_THREAD_COUNT, worker_thread);

    /* Event polling code begins */
    struct epoll_event listen_event;
    struct epoll_event *events;
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("Epoll create");
        exit(EXIT_FAILURE);
    }

    /* Server's socket for IN events */
    listen_event.data.fd = server_sock;
    listen_event.events = EPOLLIN | EPOLL_TRIGGER_TYPE;

    int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &listen_event);
    if (ret == -1)
    {
        perror("Epoll Ctl Add");
        exit(EXIT_FAILURE);
    }

    events = calloc(MAX_EPOLL_EVENTS, sizeof(struct epoll_event));
    /* Event loop */
    while (1)
    {
        int i;
        int no_events = epoll_wait (epoll_fd, events, MAX_EPOLL_EVENTS, -1);
        for (i = 0; i < no_events; i++)
        {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP))
            {
                epoll_conn_state* con = events[i].data.ptr;
                Close(con->client_fd);
                Close(con->worker_fd);
                Free(con);
                dbg_printf ("epoll error\n");
            }
            else if ((events[i].events & EPOLLIN) &&
                    (events[i].data.fd == server_sock))
            {
                static int count = 0;
                while (1)
                {
                    dbg_printf("MASTER: New connection %d\n", count++);
                    int cli_fd = accept(server_sock, NULL, NULL);
                    if (cli_fd == -1)
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            break;
                        else
                        {
                            perror("Client accept");
                            exit(EXIT_FAILURE);
                        }
                    }
                    add_client_worker_fd_to_epoll(epoll_fd, cli_fd, -1 , EVENT_OWNER_CLIENT);
                }
            }
            else if ((events[i].events & EPOLLIN))
            {
                epoll_conn_state* con = events[i].data.ptr;
                if (con->type == EVENT_OWNER_WORKER)
                {
                    /* Worker is ready with the output.
                     * Send the output to the client */
                    handle_client_response(epoll_fd, con);
                    Close(con->client_fd);
                    Close(con->worker_fd);
                    Free(con);
                }
                else
                {
                    dbg_printf("Sending request to worker\n");
                    /* Client's input is ready. Serve the HTTP request */
                    handle_client_request(epoll_fd, con);
                }
            }
            else
            {
                dbg_printf("Unknown Event type\n");
                exit(EXIT_FAILURE);
            }
        }
    }
}
