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

    char response_str[100];
    strcat(response_str, "HTTP/1.0 200 OK\r\n");
    strcat(response_str, timeString);
    strcat(response_str, "\r\n\0");
    write(fd, response_str, strlen(response_str));
}
