#include <stdio.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include "http_header.h"
#include "http.h"
#include "util.h"
#include <sys/epoll.h>

#define DEFAULT_LISTEN_PORT 80
#define MAX_LISTEN_QUEUE 100
#define MAX_NAME_LENGTH 100
#define MAX_READ_LENGTH 4096
#define MAX_FD_LIMIT 100000
#define MAX_EPOLL_EVENTS 100
#define WORKER_THREAD_PORT 9898
#define TRUE 1
#define WORKER_THREAD_COUNT 4

#define EPOLL_TRIGGER_TYPE EPOLLET

void add_client_worker_fd_to_epoll(int epollfd, int cli_fd, int worker_fd, int type)
{
    struct epoll_event event;
    epoll_conn_state* conn = malloc(sizeof(epoll_conn_state));
    conn->client_fd = cli_fd;
    conn->worker_fd = worker_fd;
    conn->type = type;
    make_socket_non_blocking(cli_fd);

    event.data.ptr = conn;
    event.events = EPOLLIN | EPOLL_TRIGGER_TYPE;

    int fd_to_add = cli_fd;
    if (type == EVENT_OWNER_WORKER)
        fd_to_add = worker_fd;

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
        printf("inet_pton error occured\n");
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

    printf("%s %s %s\n", header.request_url, header.request_type, header.request_http_version);

    request_item* reqitem = create_request_item(REQUEST_TYPE_DYNAMIC_CONTENT,
                                                        header.request_url);
    printf("Work item created\n");
    int worker_fd = send_to_worker_thread(reqitem);
    printf("Sent to worker thread\n");
    make_socket_non_blocking(worker_fd);
    con->worker_fd = worker_fd;

    add_client_worker_fd_to_epoll(epollfd, con->client_fd, worker_fd, EVENT_OWNER_WORKER);
    free_kvpairs_in_header(&header);
}

void handle_client_response(int epollfd, epoll_conn_state* con)
{
}

void worker_thread(void* arg)
{
    int thread_id = *(int*)arg;
    free(arg);
    int server_sock = create_listen_tcp_socket(WORKER_THREAD_PORT, MAX_LISTEN_QUEUE, TRUE);
    /* Sequential server */
    while (1)
    {
        int client_fd = Accept(server_sock, NULL, NULL);
        printf("Got an FD at worker thread\n");
        request_item item;
        read(client_fd, &item, sizeof(request_item));
        printf("Got request item for %s at %d\n", item.resource_url, thread_id);
    }
}

int main(int argc, char *argv[])
{
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
        perror("Epoll ctl");
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
                fprintf (stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }
            else if ((events[i].events & EPOLLIN) &&
                    (events[i].data.fd == server_sock))
            {
                printf("New connection\n");
                int cli_fd = Accept(server_sock, NULL, NULL);
                if (cli_fd == -1)
                {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                        break;
                    else
                    {
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                }
                add_client_worker_fd_to_epoll(epoll_fd, cli_fd, -1 , EVENT_OWNER_CLIENT);
            }
            else if ((events[i].events & EPOLLIN))
            {
                epoll_conn_state* con = events[i].data.ptr;
                if (con->type == EVENT_OWNER_WORKER)
                {
                    /* Worker is ready with the output.
                     * Send the output to the client */
                    handle_client_response(epoll_fd, con);
                }
                else
                {
                    /* Client's input is ready. Serve the HTTP request */
                    handle_client_request(epoll_fd, con);
                }
            }
            else
            {
                printf("UNKNOWN\n");
            }
        }
    }
}
