#include <stdio.h>
#include <stdlib.h>

int main()
{
   int i, n;
   time_t t;

   /* Intializes random number generator */
   srand((unsigned) time(&t));

   /* Print 5 random numbers from 0 to 49 */
   for( i = 0 ; i < 1000 ; i++ )
   {
      printf("%d", rand() % 50);
   }
   return(0);
}
