# Dynamo: High Performance Web Server for Dynamic Content Delivery
A high performance event driven web server designed to avoid overheads
associated with traditional CGI servers (i.e fork and exec).
Dynamo uses a thread pool for handling dynamic requests.
The main event loop accepts connections quickly and passes
the requests to threads in the thread pool.
Static pages are not a common case for this server and handled seperately
from the dynamic requests, i.e a thread is forked for every connection.
Dynamo supports code cache for recently accessed dynamic .so modules.
It also runs periodic revalidation of loaded .so files with that of the
underlying .so files and reloads libraries if they are changed.

### Configurations
* Dynamic .so libraries should be present at `CGIBIN_DIR_NAME`
* Static files like images, txt, html should be present at `STATIC_DIR_NAME`
* Both of the above constants are defined in `util.h`
* Thread pool can be configured at `WORKER_THREAD_COUNT` in `server.c`
* Cache size can be configured at `MAX_CACHE_SIZE` in `cache.h`
* Cache (.so module) revalidation time can be changed at `CACHE_REVALIDATION_TIMEOUT`
  in `util.h`
* Statistics reporter's periodicity can be configured at `STAT_INTERVAL`
  in `util.h`
* By default, `MAX_FD_LIMIT, MAX_LISTEN_QUEUE, MAX_EPOLL_EVENTS`
are configured for `100000`. Please change this to suit your workloads and server capabilities.

### Dynamic modules (for dynamic content)
Dynamic content generation programs are present in the `cgi-bin` folder.
All the programs with `.so` extensions are eligible to run in the web server.
Server dynamically loads these shared object files and executes them.
Each file corresponds directly to the URL.
For example: vamshi.so corresponds to /cgi-bin/vamshi.
Server will load vamshi.so and executes the below function on request to
/cgi-bin/vamshi

(Caching is done to avoid reperated loading of modules)

Every library code (.so) should have a function of this form
`void cgi_function (int fd, char** args)` where fd is the client's fd. Function
instead of writing the output to stdout, it has to write to this client'd fd.
Server searches the function of this declaration and executes it.

### Static content
Simply place the files in the `STATIC_DIR_NAME` folder

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

