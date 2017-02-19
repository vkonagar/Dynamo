#include<stdio.h>
int main()
{
    int r = rand();
    char* p = "This is a test page... Test Page...";
    int len = strlen(p);

    char string[200];
    sprintf(string, "%s", "HTTP/1.0 200 OK\n");
    sprintf(string, "%s%s", string, "Content-Type: text/html\n\n");
    sprintf(string, "%s%s", string, "<html><head>\n");
    sprintf(string, "%s%s", string, "<body>\n");
    sprintf(string, "%s%s", string, "<h1>");
    printf("%s", string);
    int i;
    for(i=0;i<r%1000;i++)
    {
        printf("%s", p);
    }
    char* end = "</h1> </body> </html>\r\r";
    printf("%s\n", end);
}
