#include<stdio.h>


void cgi_function(int fd)
{
    int i;
    for(i=0;i<100000;i++)
    {
        write(fd, "Hey", 3);
    }
}
