/* Author: Hari Caushik
 * Date Last Modified: 2/23/12
 * Description: Finds all primes within a range of a 32-bit number 
*/

#include <stdlib.h>
#include <math.h>
#include <strings.h>

#define BYTES 100/*536870912*/

struct compFinder {
	char *nlist;
	int total;
	int prime;
	int threadNo;
};

void printBitMap(char * nlist, char mask[8], int start, int end);

int main(int argc, char *argv[])
{
	char *nlist;		/* list of numbers up to max value */
	char mask[] = { 1 << 7, 1 << 6, 1 << 5, 1 << 4, 1 << 3, 1 << 2, 1 << 1,
			1 << 0 };
	
	int i;
	int nthreads;
	
	if (argc != 2)
	{
		printf("Indicate the number of threads.\n");
	}

	nthreads = atoi(argv[1]);
	nlist = malloc(BYTES);
	bzero(nlist, BYTES);
	
	/* mark the number 1 as special */
	nlist[1] = nlist[1] | mask[1];
	nlist[4] = nlist[4] | mask[4];
	if (nlist[1] & mask[1])
		printf("Number 1 has been marked as special.\n");
	for (i = 0; i < 8; i++)
	{
		printf("%d ", (nlist[i] & mask[i]) > 0);
	}
	printf("\n");

	/* mark all even numbers */
	for (i = 4; i < BYTES; i += 2)
	{
		int byte;
		int bit;

		byte = i / 8;
		bit = i % 8;
		nlist[byte] = nlist[byte] | mask[bit];
	}
	printBitMap(nlist, mask, 1, 15);
	free(nlist);
	return 0;
}

void printBitMap(char *nlist, char mask[8],  int start, int end)
{
	int i;
	for (i = start; i <= end; i++)
	{
		int byte;
		int bit;

		byte = i / 8;
		bit = i % 8;
		printf("%d ", (nlist[byte] & mask[bit]) > 0);
	}
	printf("\n");

}
