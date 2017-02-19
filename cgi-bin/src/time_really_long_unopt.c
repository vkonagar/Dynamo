#include<stdio.h>
#include<string.h>
#include <time.h>

int main(){
    time_t current_time;
    struct tm * time_info;
    char timeString[9];  // space for "HH:MM:SS\0"

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);

    char string[100000];
    sprintf(string, "%s", "HTTP/1.0 200 OK\n");
    sprintf(string, "%s%s", string, "Content-Type: text/html\n\n");
    sprintf(string, "%s%s", string, "<html><head>\n");
    sprintf(string, "%s%s", string, "<title>Hello, world!</title>");
    sprintf(string, "%s%s", string, "</head>\n");
    sprintf(string, "%s%s", string, "<body>\n");
    sprintf(string, "%s%s", string, "<h1>");

    srand(time(NULL));
    int r = rand();    //returns a pseudo-random integer between 0 and RAND_MAX
    int i;
    for(i=0;i<r%2000;i++)
    {
        sprintf(string, "%s%s", string, timeString);
    }
    sprintf(string, "%s%s", string, "</h1>\n");
    sprintf(string, "%s%s", string, "</body> </html>\r\n");
    printf("%s\n", string);
}
