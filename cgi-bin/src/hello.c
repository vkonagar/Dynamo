#include<stdio.h>

void cgi_function(int fd)
{
    write(fd, "Hello world", strlen("Hello world"));
}
