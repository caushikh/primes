/* Author: Hari Caushik
 * Date Last Modified: 2/23/12
 * Description: Finds all primes within a range of a 32-bit number 
*/

#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>

#define BYTES 100/*536870912*/

struct compFinder {
	unsigned char *nlist;
	int total;
	int prime;
	int threadNo;
};

void printBitMap(unsigned char * nlist, unsigned char mask[8], int start, int end);

int main(int argc, char *argv[])
{
	unsigned char *nlist;		/* list of numbers up to max value */
	unsigned char mask[] = { 1 << 7, 1 << 6, 1 << 5, 1 << 4, 1 << 3, 1 << 2,
				 1 << 1, 1 << 0 };
	
	int i, j, k;
	int byte, bit;
	int nthreads;
	pthread_t *pfinder;
	struct compFinder *cfinder;
	
	if (argc != 2)
	{
		printf("Indicate the number of threads.\n");
		exit(EXIT_FAILURE);
	}

	nthreads = atoi(argv[1]);
	nlist = malloc(BYTES);
	bzero(nlist, BYTES);
	pfinder = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	cfinder = (struct compFinder *) malloc(sizeof(struct compFinder));
	
	/* mark the number 1 as special */
	nlist[0] = nlist[0] | mask[1];

	/* mark all even numbers */
	for (i = 4; i < BYTES; i += 2)
	{
		byte = i / 8;
		bit = i % 8;
		nlist[byte] = nlist[byte] | mask[bit];
	}

	/* perform sieve, mark off composites */
	for (i = 2; i < sqrt(BYTES);)
	{
		/* find first unmarked number */
		j = i;
		while(j < BYTES)
		{
			byte = j / 8;
			bit = j % 8;
			if (!(nlist[byte] & mask[bit]))
			{
				break;
			}
			j++;
		}
		if (j >= BYTES)
			break;

		/* mark off composites in parallel */
		for (k = 0, k < nthreads, k++)
		{
			if (pthread_create(&pfinder[k], NULL, findComposites, (void *)cfinder) != 0)
			{
				printf("Threads could not be created.\n");
				free(pfinder);
				free(cfinder);
				free(nlist);
				exit(EXIT_FAILURE);
			}
		}

		for (k = 0; k < nthreads; k++)
			pthread_join(pfinder[k], NULL); 
		i = ++j; 
	}
	free(nlist);
	free(pfinder);
	free(cfinder);
	return 0;
}

void printBitMap(unsigned char *nlist, unsigned char mask[8],  int start, int end)
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

void *findComposites(void *findargs)
{
	return 0;
}
