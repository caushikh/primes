/* Author: Hari Caushik
 * Date Last Modified: 2/23/12
 * Description: Finds all primes within a range of a 32-bit number 
*/

#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
	int num = pow(2, 3) - 1;
	int set = ~(1 << 2);
	printf("The number is %d.\n", num & set);


	return 0;
}
