#include <stdio.h>
#include <time.h>
#include <openssl/md5.h>

void cgi_function(int fd)
{

    time_t current_time;
    struct tm * time_info;
    char timeString[9];  // space for "HH:MM:SS\0"

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(timeString, sizeof(timeString), "%H:%M:%S", time_info);

    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext, timeString, strlen(timeString));
    MD5_Final (c, &mdContext);
    int i;
    for(i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
        char buf[10];
        sprintf(buf, "%02x", c[i]);
        write(fd, buf, strlen(buf));
	}
}

int main()
{
    execute(0);
}
