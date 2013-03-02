/* Author: Hari Caushik
 * Date Last Modified: 2/23/12
 * Description: Finds all primes within a range of a 32-bit number 
*/

#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>

#define BYTES 15/*536870912*/
#define TOTAL BYTES*8-1

struct compFinder {
	unsigned char *nlist;
	unsigned char mask[8];
	int prime;
	int threadNo;
	int nthreads;
	int total;
};

pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t b_mutex[BYTES];

void printBitMap(struct compFinder *, int, int );
void *findComposites(void *findargs);

int main(int argc, char *argv[])
{
	//unsigned char *nlist;		/* list of numbers up to max value */
	unsigned char mask[] = { 1 << 7, 1 << 6, 1 << 5, 1 << 4, 1 << 3, 1 << 2,
				 1 << 1, 1 << 0 };
	int i, j, k;
	long m;
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
//	nlist = malloc(BYTES);
//	bzero(nlist, BYTES);
	pfinder = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	cfinder = (struct compFinder *) malloc(sizeof(struct compFinder));
	cfinder->nlist = (unsigned char *) malloc(BYTES);
	cfinder->nthreads = atoi(argv[1]);
	bzero(cfinder->nlist, BYTES);
	/* initialize byte locks */
	for (m = 0; m < BYTES; m++)
	{
		pthread_mutex_init(&b_mutex[m], NULL);
	}
	/* initialize mask */
	for (i = 0; i < 8; i++)
	{
		cfinder->mask[i] = 1 << (8-i-1);
	}

	cfinder->total = BYTES * 8 - 1;
	
	/* mark the number 1 as special */
	cfinder->nlist[0] = cfinder->nlist[0] | cfinder->mask[1];

	/* mark all even numbers */
	for (i = 4; i < BYTES; i += 2)
	{
		byte = i / 8;
		bit = i % 8;
		cfinder->nlist[byte] = cfinder->nlist[byte] | cfinder->mask[bit];
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
			if (!(cfinder->nlist[byte] & cfinder->mask[bit]))
			{
				break;
			}
			j++;
		}
		if (j >= BYTES)
			break;
		cfinder->prime = j;

		/* mark off composites in parallel */
		for (k = 0; k < nthreads; k++)
		{
			if (pthread_create(&pfinder[k], NULL, findComposites, (void *)cfinder) != 0)
			{
				printf("Threads could not be created.\n");
				free(pfinder);
				free(cfinder->nlist);
				free(cfinder);
				exit(EXIT_FAILURE);
			}
		}

		for (k = 0; k < nthreads; k++)
			pthread_join(pfinder[k], NULL); 
		i = ++j; 
	}
	printBitMap(cfinder, 1, 24);

	free(cfinder->nlist);
	free(pfinder);
	free(cfinder);
	return 0;
}

void printBitMap(struct compFinder *cfinder,  int start, int end)
{
	int i;
	int byte;
	int bit;
	for (i = start; i <= end; i++)
	{
		byte = i / 8;
		bit = i % 8;
		printf("%d ", (cfinder->nlist[byte] & cfinder->mask[bit]) > 0);
	}
	printf("\n");

}

void *findComposites(void *comp)
{
	struct compFinder *compstruct = (struct compFinder *) comp;
	pthread_mutex_lock(&p_mutex);
	int temp = compstruct->threadNo;
	compstruct->threadNo++;
	pthread_mutex_unlock(&p_mutex);
	int nmult = compstruct->total / compstruct->prime;
	long mpert = nmult / compstruct->nthreads;
	int start = (temp * mpert + 2) * compstruct->prime;
	int end;
	int i;
	int byte;
	int bit;
	if (temp == (compstruct->nthreads - 1))
	{
		end = nmult * compstruct->prime;
		printf("end: %d\n", end);
	}
	else
	{
		end = ((temp + 1) * mpert + 1) * compstruct->prime;
	}
	for (i = start; i <= end; i += compstruct->prime)
	{
		byte = i / 8;
		pthread_mutex_lock(&b_mutex[byte]);
		bit = i % 8;
		compstruct->nlist[byte] = compstruct->nlist[byte] | compstruct->mask[bit];
		pthread_mutex_unlock(&b_mutex[byte]);
	}


	return 0;
}
