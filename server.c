/*
 * HTTP/1.0 compliant high performance dynamic content server.
 *
 * Features
 * ********
 * 1. Implements HTTP/1.0 GET requests for static and dynamic content.
 * 2. Assumes one connection per request (no persistent connections).
 * 3. Uses worker threads with dynamic loading of (.so) to achieve faster dynamic
	content generation. Only ELF compatible modules are supported.
 * 4. Serves HTML (.html), image (.gif and .jpg), and text (.txt) files.
 * 5. Accepts a single command-line argument: the port to listen on.
 * 6. Implements concurrency using IO Multiplexing and worker threads.
 *
 * Author: Vamshi Reddy Konagari
 * Email: vkonagar@andrew.cmu.edu
 * Date: 2/19/2017
 */
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
#include "http_util.h"
#include "util.h"
#include <sys/epoll.h>
#include "csapp.h"
#include <dlfcn.h>

#define DEFAULT_LISTEN_PORT         80      /* Server's port */
#define MAX_LISTEN_QUEUE            100000  /* Avoids connection resets */
#define MAX_FD_LIMIT                100000
#define MAX_EPOLL_EVENTS            100000  /* Determines how many outstanding
                                              events in the system that can
                                              be delivered. Set this to around
                                              max connections that can be
                                              outstanding in the server */
#define WORKER_THREAD_COUNT         4       /* Tune this parameter according
                                               to number of cores in your
                                               system */
/*
 * This is a thread function to serve static content like images, text, html.
 * Upon a request for static content, a thread is spawned with this function
 * to server the request. It uses sendfile to avoid overhead of kernel->user
 * copying. */
void* static_content_worker_thread(void* arg)
{
    request_item* item = (request_item*)arg;
    /* This thread is independent, its resources like stack, etc should
     * be freed automatically. */
    if (pthread_detach(pthread_self()) == -1)
    {
        perror("Thread cannot be detached");
        return (void*)-1;
    }
    handle_static(item->client_fd, item->resource_name);
    Close(item->client_fd);
    free(item);
    return 0;
}

/* This is a client request handler.
 * @param epollfd IO multiplexed fd.
 * @param con connection state of the client's connection in epoll.
 * */
void handle_client_request(int epollfd, epoll_conn_state* con)
{
    /* Scan the header */
    http_header_t header;
    init_header(&header);
    http_scan_header(con->client_fd, &header);

    request_item* reqitem;
    char resource_name[MAX_RESOURCE_NAME_LENGTH]; /* ex: cmu.jpg, etc */

    switch (get_resource_type(header.request_url, resource_name))
    {
        case RESOURCE_TYPE_CGI_BIN:
                    reqitem = create_dynamic_request_item(resource_name);
                    /* Dynamic requests are handled by worker threads */
                    int worker_fd = send_to_worker_thread(reqitem);
                    Free(reqitem);
                    /* Store the assigned worker to the client connection's
                     * epoll state */
                    con->worker_fd = worker_fd;
                    make_socket_non_blocking(worker_fd);
                    add_worker_fd_to_epoll(epollfd, worker_fd, con);
                    break;
        case RESOURCE_TYPE_UNKNOWN:
                    dbg_printf("Unknown %s\n", header.request_url);
                    break;
        default:    /* Handle static
                     * A worker is created for this request */
                    create_static_worker(con->client_fd,
                                        static_content_worker_thread,
                                        resource_name);
                    Free(con);
                    break;
    }
    /* Free the header */
    free_kvpairs_in_header(&header);
}

/*
 * Handle the response from the worker thread and send it back to the client.
 * It is invoked for dynamic requests after the worker thread finishes
 * generating the dynamic content */
int handle_client_response(int epollfd, epoll_conn_state* con)
{
    /* read the generated content and write back to the client */
    char buf[MAX_READ_LENGTH];
    int read_count = 0;
    while ((read_count = read(con->worker_fd, buf, MAX_READ_LENGTH)) > 0)
    {
        int a = rio_writen(con->client_fd, buf, read_count);
    }
    if (read_count == -1 && errno != EAGAIN)
    {
        perror("Error in handle_client_response");
        exit(EXIT_FAILURE);
    }
    else if(read_count == 0)
    {
        /* When all of the data from the socket is read */
        dbg_printf("Phew! Done reading all the data.. \n");
        return RESPONSE_HANDLING_COMPLETE;
    }
    else if(read_count == -1 && errno == EAGAIN)
    {
        /* Non blocking err */
        dbg_printf("Not complete.. will try again\n");
    }
    return RESPONSE_HANDLING_PARTIAL;
}


/*
 * This function is invoked on a worker thread to serve dynamic content.
 * It has a internal TCP server to which master connects and passes the requests
 * This generates the required dynamic content and writes
 * the data back to the master's socket
 */
void* dynamic_content_worker_thread(void* arg)
{
    /* This thread is independent, its resources like stack, etc should
     * be freed automatically. */
    if (pthread_detach(pthread_self()) == -1)
    {
        perror("Thread cannot be detached");
        return (void*)-1;
    }

    /* TCP server to which master sends requests */
    int server_sock = create_listen_tcp_socket(WORKER_THREAD_PORT,
                                            MAX_LISTEN_QUEUE, SHARED_SOCKET);
    /* Sequential server */
    request_item item;
    while (1)
    {
        int client_fd = accept(server_sock, NULL, NULL);
        if (client_fd == -1)
            continue;
        /* Read the request from the master */
        int r = read(client_fd, &item, sizeof(request_item));
        /* Load the module and generate the content */
        handle_dynamic_exec_lib(client_fd, item.resource_name);
        Close(client_fd);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    init_library();

    increase_fd_limit(MAX_FD_LIMIT);
    signal(SIGPIPE, SIG_IGN); /* Ignore Sigpipe */
    int port = parse_port_number(argc, argv[1]);
    if (port == -1)
    {
        fprintf(stderr, "Using default port number %d\n", DEFAULT_LISTEN_PORT);
        port = DEFAULT_LISTEN_PORT;
    }

    /* Create a new server socket */
    int server_sock = create_listen_tcp_socket(port, MAX_LISTEN_QUEUE,
                                                    NON_SHARED_SOCKET);
    make_socket_non_blocking(server_sock);

    /* Create dynamic content generation workers */
    create_threads(WORKER_THREAD_COUNT, dynamic_content_worker_thread);

    /* Create statistics thread to print requests and replies rate*/
    create_stat_thread();

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
    memset(&listen_event, 0, sizeof(listen_event));
    listen_event.data.fd = server_sock;
    listen_event.events = EPOLLIN | EPOLLET; /* Edge triggered because we
                                                want to get notified only
                                                when a new connection is
                                                accepted */

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
                switch(con->type)
                {
                    case EVENT_OWNER_WORKER:    Close(con->worker_fd);
                                                /* Fall through */
                    case EVENT_OWNER_CLIENT:    Close(con->client_fd);
                                                Free(con);
                                                break;
                    default:                    Close(events[i].data.fd);
                }
            }
            else if ((events[i].events & EPOLLIN) &&
                    (events[i].data.fd == server_sock))
            {
                /* Server's Listening socket. Possible new conenction.
                 * Accept all of the pending conenctions */
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
                    increment_request_count(); /* For stats */
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
                    if (handle_client_response(epoll_fd, con) ==
                                                RESPONSE_HANDLING_COMPLETE)
                    {
                        Close(con->client_fd);
                        Close(con->worker_fd);
                        Free(con->client_con);
                        Free(con);
                        increment_reply_count();
                    }
                }
                else
                {
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
