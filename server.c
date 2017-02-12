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

#define MAX_READ_LENGTH         4096000
#define DEFAULT_LISTEN_PORT     80
#define MAX_LISTEN_QUEUE        10000
#define MAX_NAME_LENGTH         100
#define MAX_FD_LIMIT            100000
#define MAX_EPOLL_EVENTS        10000

#define WORKER_THREAD_COUNT     3

#define RESPONSE_HANDLING_COMPLETE  1
#define RESPONSE_HANDLING_PARTIAL   2

void handle_client_request(int epollfd, epoll_conn_state* con)
{
    dbg_printf("Handling client request\n");
    http_header_t header;
    init_header(&header);
    http_scan_header(con->client_fd, &header);
    request_item* reqitem = create_request_item(REQUEST_TYPE_DYNAMIC_CONTENT,
                                                        header.request_url);
    int worker_fd = send_to_worker_thread(reqitem);
    free(reqitem);

    con->worker_fd = worker_fd;
    make_socket_non_blocking(worker_fd);
    add_worker_fd_to_epoll(epollfd, worker_fd, con->client_fd);
    free_kvpairs_in_header(&header);
}

int handle_client_response(int epollfd, epoll_conn_state* con)
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
    if (read_count == -1 && errno != EAGAIN)
    {
        perror("Error in handle_client_response");
        exit(EXIT_FAILURE);
    }
    else if(read_count == 0)
    {
        dbg_printf("Phew! Read all the data.. \n");
        return RESPONSE_HANDLING_COMPLETE;
    }
    else if(read_count == -1 && errno == EAGAIN)
    {
        dbg_printf("Not complete.. will try again\n");
    }
    return RESPONSE_HANDLING_PARTIAL;
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
    listen_event.events = EPOLLIN | EPOLLET;

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
                if (con->type == EVENT_OWNER_CLIENT ||
                        con->type == EVENT_OWNER_WORKER)
                {
                    Close(con->client_fd);
                    Close(con->worker_fd);
                    Free(con);
                }
                else
                {
                    Close(events[i].data.fd);
                }
                dbg_printf ("epoll error\n");
            }
            else if ((events[i].events & EPOLLIN) &&
                    (events[i].data.fd == server_sock))
            {
                while (1)
                {
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
                    add_client_fd_to_epoll(epoll_fd, cli_fd);
                }
            }
            else if ((events[i].events & EPOLLIN))
            {
                epoll_conn_state* con = events[i].data.ptr;
                if (con->type == EVENT_OWNER_WORKER)
                {
                    /* Worker is ready with the output.
                     * Send the output to the client */
                    dbg_printf("Event worker\n");
                    if (handle_client_response(epoll_fd, con) == RESPONSE_HANDLING_COMPLETE)
                    {
                        printf("Closing\n");
                        Close(con->client_fd);
                        Close(con->worker_fd);
                        Free(con);
                    }
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
