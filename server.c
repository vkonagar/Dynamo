/*
 * HTTP/1.0 compliant concurrent server
 *
 * Features
 * ********
 * 1. Implements HTTP/1.0 GET requests for static and dynamic content.
 * 2. Assumes one connection per request (no persistent connections).
 * 3. Uses the CGI protocol to serve dynamic content.
 * 4. Serves HTML (.html), image (.gif and .jpg), and text (.txt) files.
 * 5. Accepts a single command-line argument: the port to listen on.
 * 6. Implements concurrency using threads.
 */

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include "http_header.h"
#include "csapp.h"

#define DEFAULT_LISTEN_PORT 80
#define MAX_LISTEN_QUEUE 100

void* client_handler(void* arg)
{
    int fd = (int)arg;

    /* This thread is independent, its resources like stack, etc should
     * be freed automatically. */
    if (pthread_detach(pthread_self()) == -1)
    {
        perror("Thread cannot be detached");
        return (void*)-1;
    }

    http_header_t header;
    init_header(&header);
    scan_header(fd, &header);
    printf("HTTP request is %s %s %s\n", header.request_type,
                                        header.request_url,
                                        header.request_http_version);



    close(fd);
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_LISTEN_PORT;
    if (argc == 2)
    {
        errno = 0;
        port = strtol(argv[1], NULL, 10);
        if (((port == LONG_MIN || port == LONG_MAX) && errno == ERANGE) || port == 0)
        {
            printf("Provide a valid port number\n");
        }
    }
    else
    {
        printf("Port not provided. Using the default port %d\n", DEFAULT_LISTEN_PORT);
    }

    /* Create a server socket */
    int server_sock = Socket(AF_INET, SOCK_STREAM, 0);

    /* Bind the socket to a port */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    Bind(server_sock, (struct sockaddr*) &server_addr, sizeof(server_addr));

    Listen(server_sock, MAX_LISTEN_QUEUE);

    /* Accept concurrent connections */
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    int size = 0;
    pthread_t thread_id;
    while (1)
    {
        int client_fd = Accept(server_sock, (struct sockaddr*) &client_addr, &size);
        pthread_create(&thread_id, NULL, client_handler, (void*) client_fd);
        printf("Connection from a client\n");
    }
    close(server_sock);
}
