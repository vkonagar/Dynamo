#include<stdio.h>
#include<string.h>
#include <time.h>

void cgi_function(int fd)
{
    time_t current_time;
    struct tm * time_info;
    char timeString[9];  // space for "HH:MM:SS\0"

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);

    char string[600];
    sprintf(string, "%s", "HTTP/1.0 200 OK\n");
    sprintf(string, "%s%s", string, "Content-Type: text/html\n\n");
    sprintf(string, "%s%s", string, "<html><head>\n");
    sprintf(string, "%s%s", string, "<title>Hello, Vamshi Reddy!</title>");
    sprintf(string, "%s%s", string, "</head>\n");
    sprintf(string, "%s%s", string, "<body>\n");
    sprintf(string, "%s%s", string, "<h1>");
    sprintf(string, "%s%s", string, timeString);
    sprintf(string, "%s%s", string, "</h1>\n");
    sprintf(string, "%s%s", string, "</body> </html>\r\n");
    write(fd, string, strlen(string));
}
