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
#include "http_util.h"
#include "csapp.h"
#include "util.h"
#include <sys/resource.h>

#define DEFAULT_LISTEN_PORT 80
#define MAX_LISTEN_QUEUE 100
#define MAX_NAME_LENGTH 100
#define MAX_READ_LENGTH 4096
#define MAX_FD_LIMIT 100000

#ifdef DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbg_printf(...)
#endif

static void handle_dynamic(int fd, char* resource_name)
{
    int path_len = MAX_DLL_NAME_LENGTH + strlen(CGIBIN_DIR_NAME) + MAX_PATH_CHARS;
    char lib_path[path_len];
    snprintf(lib_path, path_len, "./%s/%s", CGIBIN_DIR_NAME, resource_name);

    int pipe_fds[2];
    if (pipe(pipe_fds) == -1)
    {
        perror("pipe");
    }

    if (fork() == 0)
    {
        /* Child */
        close(STDOUT_FILENO);
        close(pipe_fds[0]);
        dup(pipe_fds[1]);
        char *newargv[] = { resource_name, NULL };
        if (execve(lib_path, newargv, NULL) == -1)
        {
            http_write_response_header(fd, HTTP_404);
            perror("exec");
            exit(EXIT_FAILURE);
        }
    }

    /* Parent. Wait for the child to finish */
    int status;
    if (wait(&status) == -1)
        perror("wait");

    close(pipe_fds[1]);

    if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_FAILURE)
    {
        http_write_response_header(fd, HTTP_200);
        char buf[MAX_READ_LENGTH];
        int read_cnt;
        while((read_cnt = read(pipe_fds[0], buf, MAX_READ_LENGTH)) > 0)
        {
            write(fd, buf, read_cnt);
        }
    }
    close(pipe_fds[0]);
}

void* client_handler(void* arg)
{
    int fd = *((int*)arg);
    free(arg);
    /* This thread is independent, its resources like stack, etc should
     * be freed automatically. */
    if (pthread_detach(pthread_self()) == -1)
    {
        perror("Thread cannot be detached");
        return (void*)-1;
    }

    http_header_t header;
    init_header(&header);
    http_scan_header(fd, &header);

    char resource_name[MAX_NAME_LENGTH];
    switch (get_resource_type(header.request_url, resource_name))
    {
        case RESOURCE_TYPE_CGI_BIN: handle_dynamic(fd, resource_name);
                                    break;
        case RESOURCE_TYPE_HTML:
        case RESOURCE_TYPE_TXT:
        case RESOURCE_TYPE_GIF:
        case RESOURCE_TYPE_JPG:
                                    handle_static(fd, resource_name);
                                    break;
        case RESOURCE_TYPE_UNKNOWN: dbg_printf("Unknown request type\n");
                                    break;
    }
    increment_reply_count();
    free_kvpairs_in_header(&header);
    close(fd);
}

int main(int argc, char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    /* Set resource limits */
    struct rlimit res;
    res.rlim_cur = MAX_FD_LIMIT;
    res.rlim_max = MAX_FD_LIMIT;
    if( setrlimit(RLIMIT_NOFILE, &res) == -1 )
    {
	    perror("Resource FD limit");
	    exit(0);
    }

    int port = DEFAULT_LISTEN_PORT;
    if (argc == 2)
    {
        errno = 0;
        port = strtol(argv[1], NULL, 10);
        if (((port == LONG_MIN || port == LONG_MAX) && errno == ERANGE) || port == 0)
        {
            dbg_printf("Provide a valid port number\n");
        }
    }
    else
    {
        printf("Port not provided. Using the default port %d\n", DEFAULT_LISTEN_PORT);
    }

    /* Initialize stat mutex */
    init_stat_mutexes();
    /* Create stat thread */
    create_stat_thread();

    int server_sock = create_listen_tcp_socket(port, MAX_LISTEN_QUEUE, NON_SHARED_SOCKET);
    /* Accept concurrent connections */
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    int size = 0;
    pthread_t thread_id;
    while (1)
    {
        int* client_fd = malloc(sizeof(int));
        *client_fd = Accept(server_sock, (struct sockaddr*) &client_addr, &size);
        pthread_create(&thread_id, NULL, client_handler, (void*) client_fd);
        increment_request_count();
    }
    close(server_sock);
}
