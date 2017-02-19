#include<stdio.h>
#include<string.h>
#include <time.h>

void cgi_function(int fd)
{
    char string[100000];
    sprintf(string, "%s", "HTTP/1.0 200 OK\n");
    sprintf(string, "%s%s", string, "Content-Type: text/html\n\n");
    sprintf(string, "%s%s", string, "<html><head>\n");
    sprintf(string, "%s%s", string, "<title>Hello, world!</title>");
    sprintf(string, "%s%s", string, "</head>\n");
    sprintf(string, "%s%s", string, "<body>\n");
    sprintf(string, "%s%s", string, "<h1>");
    int i;
    for(i=0;i<1000;i++)
    {
        sprintf(string, "%s%s", string, "VA");
    }
    sprintf(string, "%s%s", string, "</h1>\n");
    sprintf(string, "%s%s", string, "</body> </html>\r\n");
    write(fd, string, strlen(string));
}
