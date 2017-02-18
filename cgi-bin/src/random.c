#include <stdio.h>
#include <stdlib.h>

void cgi_function(int fd)
{
   int i, n;
   time_t t;

   /* Intializes random number generator */
   srand((unsigned) time(&t));

   /* Print 5 random numbers from 0 to 49 */
   for( i = 0 ; i < 1000 ; i++ )
   {
		char buf[10];
		sprintf(buf, "%d", rand() % 50);
		write(fd, buf, strlen(buf));
   }
}
