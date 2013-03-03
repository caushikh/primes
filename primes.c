/* Author: Hari Caushik
 * Date Last Modified: 2/23/12
 * Description: Finds all primes within a range of a 32-bit number 
*/

#include <stdlib.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdint.h>

#define BYTES 32/*536870912*/
#define TOTAL BYTES*8-1

struct compFinder {
	unsigned char *nlist;
	unsigned char mask[8];
	uint32_t prime;
	int threadNo;
	int nthreads;
	uint32_t total;
	int done;
	sem_t *find;
	sem_t *complete;
};

pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER; 

void printBitMap(struct compFinder *, int, int );
void *findComposites(void *findargs);
int getNumPrimes(struct compFinder *cfinder);

static void handler(int sig)
{
	
}

int main(int argc, char *argv[])
{
	uint32_t i, j, k;
	uint32_t m;
	uint32_t byte, bit;
	int nthreads;
	pthread_t *pfinder;
	struct compFinder *cfinder;
	sem_t *find;
	sem_t *complete;
	
	if (argc != 2)
	{
		printf("Indicate the number of threads.\n");
		exit(EXIT_FAILURE);
	}

	nthreads = atoi(argv[1]);
	find = (sem_t *) malloc(nthreads * sizeof(sem_t));
	complete = (sem_t *) malloc(nthreads * sizeof(sem_t));
	pfinder = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	cfinder = (struct compFinder *) malloc(sizeof(struct compFinder));
	cfinder->nlist = (unsigned char *) malloc(BYTES);
	if (cfinder->nlist == NULL)
	{
		free(pfinder);
		free(cfinder);
		exit(EXIT_FAILURE);
	}
	cfinder->nthreads = atoi(argv[1]);
	bzero(cfinder->nlist, BYTES);
	
	for (i = 0; i < nthreads; i++)
	{
		sem_init(&find[i], 0, 0);
		sem_init(&complete[i], 0, 0);
	}
	cfinder->find = find;
	cfinder->complete = complete;

	/* initialize mask */
	for (i = 0; i < 8; i++)
	{
		cfinder->mask[i] = 1 << (8-i-1);
	}

	cfinder->total =(uint32_t) ((unsigned long)(BYTES) * 8 - 1);
	
	/* mark the number 1 as special */
	cfinder->nlist[0] = cfinder->nlist[0] | cfinder->mask[1];
	
	/* mark all even numbers */

	for (i = 4; i < (uint32_t)cfinder->total; i += 2)
	{
		byte = i / 8;
		bit = i % 8;
		if (i < 10)
			printf("the message\n");
		cfinder->nlist[byte] = cfinder->nlist[byte] | cfinder->mask[bit];
	}

	
	/* mark off composites in parallel */
	for (k = 0; k < nthreads; k++)
	{
		if (pthread_create(&pfinder[k], NULL, findComposites, (void *)cfinder) != 0)
		{
			printf("Threads could not be created.\n");
			for (i = 0; i < nthreads; i++)
			{
				sem_destroy(&find[i]);
				sem_destroy(&complete[i]);
			}
			free(find);
			free(complete);
			free(pfinder);
			free(cfinder->nlist);
			free(cfinder);
			exit(EXIT_FAILURE);
		}
	}


	/* perform sieve, mark off composites */
	for (i = 2; i <= sqrt(cfinder->total);)
	{
		/* find first unmarked number */
		j = i;
		
		while(j < cfinder->total)
		{
			byte = j / 8;
			bit = j % 8;
			if (!(cfinder->nlist[byte] & cfinder->mask[bit]))
			{
				break;
			}
			j++;
		}
		if (j >= cfinder->total)
			break;
		cfinder->prime = j;

		/* allowing threads now to cross off composites */
		for(k = 0; k < nthreads; k++)
			sem_post(&find[k]);
	
		/* waiting for all threads to finish before getting next prime 
		 */
		for (k = 0; k < nthreads; k++)
			sem_wait(&complete[k]);
		if (j == 2)
			printf("got to 10000\n");
		i = ++j; 
	}

	cfinder->done = 1;

	/* telling threads to terminate */
	for(k = 0; k < nthreads; k++)
		sem_post(&find[k]);
	
	for (k = 0; k < nthreads; k++)
		pthread_join(pfinder[k], NULL); 

//	printBitMap(cfinder, 1, 8000);
//	printf("There are %d primes.\n", getNumPrimes(cfinder));
	printf("The program gets here. \n");
	for (i = 0; i < nthreads; i++)
	{
		sem_destroy(&find[i]);
		sem_destroy(&complete[i]);
	}
	free(find);
	free(complete);
	free(cfinder->nlist);
	free(pfinder);
	free(cfinder);
	return 0;
}

void printBitMap(struct compFinder *cfinder,  int start, int end)
{
	uint32_t i;
	uint32_t byte;
	uint32_t bit;
	for (i = start; i <= end; i++)
	{
		byte = i / 8;
		bit = i % 8;
		if (!(cfinder->nlist[byte] & cfinder->mask[bit]))
			printf("%d ", i);
	}
	printf("\n");

}

int getNumPrimes(struct compFinder *cfinder)
{
	uint32_t i;
	uint32_t byte;
	uint32_t bit;
	uint32_t pcount = 0;
	for (i = 0; i <= cfinder->total; i++)
	{
		byte = i / 8;
		bit = i % 8;
		if (!(cfinder->nlist[byte] & cfinder->mask[bit]))
			pcount++;
	}
	return pcount;
}

void *findComposites(void *comp)
{
	struct compFinder *compstruct = (struct compFinder *) comp;
	uint32_t nmult;
	uint32_t mpert;
	int temp;
	uint32_t i;
	uint32_t byte, bit;
	uint32_t start, end;
	uint32_t prime;

	/* get thread id */
	pthread_mutex_lock(&p_mutex);
	temp = compstruct->threadNo;
	compstruct->threadNo++;
	pthread_mutex_unlock(&p_mutex);

	/* wait for signal and get prime */
	while(1)
	{
		sem_wait(&compstruct->find[temp]);
		if (compstruct->done)
			break;
		prime = compstruct->prime;
		nmult = compstruct->total / prime;
		mpert = nmult / compstruct->nthreads;
		
	
		if (!mpert)
		{
			/* multiples per thread greater than the number of threads */
			for (i = 0; i < nmult - 1; i++)
			{
				if (temp == i)
				{
					start = (i + 2) * prime;
					end = (i + 2) * prime;
				}
			}
		
			for (i = nmult - 1; i < compstruct->nthreads; i++)
			{
				/* for inactive threads just make start arbitrarily
				   greater than end */
				if (temp == i)
				{
					start = 4;
					end = 2;
				}
			}
		}
		else	
		{	
			/* set start composite number */
			if (!temp)
				start = 2 * prime;
			else
			{
				start = (temp * mpert + 2) * prime;
				/* set start to first multiple in byte */
				while (1)
				{
					if ( (start / 8) != ((start - prime) / 8))
					{
						/* first multiple in byte */
						break;
					}
					start += prime;
				}
			}	

			/* set end composite number */	
			if (temp == (compstruct->nthreads - 1))
			{
				end = nmult * prime;
			}
			else
			{
				end = ((temp + 1) * mpert + 1) * prime;
				while(1)
				{
					/* set end to last multiple of byte */
					if ( ((end + prime) / 8) != (end / 8))
					{
						/* last multiple of byte */
						break;
					}
					end += prime;
				}
			}
		}

		for (i = start; i <= end; i += prime)
		{
			byte = i / 8;
			bit = i % 8;
			compstruct->nlist[byte] = compstruct->nlist[byte] | compstruct->mask[bit];
		}
		sem_post(&compstruct->complete[temp]);
	}


	return 0;
}
