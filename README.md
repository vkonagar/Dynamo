# Dynamo: High Performance Web Server for Dynamic Content Delivery
A high performance event driven web server designed to avoid overheads associated with traditional CGI servers (i.e fork and exec). Dynamo uses a thread pool for handling dynamic requests. The main event loop accepts connections quickly and passes the requests to threads in the thread pool. Static pages are not a common case for this server and handled seperately from the dynamic requests, a thread is forked for every connection.

### Configurations
* Thread pool can be configured at `WORKER_THREAD_COUNT` in `server.c`
* By default, `MAX_FD_LIMIT, MAX_LISTEN_QUEUE, MAX_EPOLL_EVENTS` are configured for `100000`. Please change this to suit your workloads and server capabilities.

### Scripts
Dynamic content generation programs are present in the `cgi-bin` folder. All the programs with `.so` extensions are eligible to run in the web server. Server dynamically loads these shared object files and executes them.

Every script should have a function of this form
`void cgi_function (int fd, char** args)`
Server searches the function of this declaration and executes it.

### Running the Server
```sh
$ sudo ./server <port>
```
There is also an unoptimized forking CGI server that is shipped along with Dynamo.
```sh
$ sudo ./server_unopt <port>
```

To enable debugging, comment `#define DEBUG` in util.h

### Testing
Apache Benchmark
```sh
$ sudo ab -c 1000 -n 100000 localhost/cgi-bin/string
# 1000 concurrent connections generating a total of 100000 connections
```

