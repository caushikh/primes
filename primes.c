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
#include <stdio.h>
#include <signal.h>
#include "listbag.h"

#define BYTES 20/*536870912*/
#define TOTAL BYTES*8-1

struct compFinder {
	unsigned char *nlist;
	unsigned char mask[8];
	uint32_t prime;
	uint32_t threadNo;
	uint32_t nthreads;
	uint32_t total;
	int done;
	sem_t *find;
	sem_t *complete;
	int happy;
	int sad;
	int phappy;
	int psad;
};

pthread_mutex_t p_mutex = PTHREAD_MUTEX_INITIALIZER; 

void printBitMap(struct compFinder *, uint32_t, uint32_t );
void *findComposites(void *findargs);
int getNumPrimes(struct compFinder *cfinder);
uint32_t calcInterm(char *buf);

static pthread_t *pfinder = NULL;
static struct compFinder *cfinder = NULL;
static sem_t *find = NULL;
static sem_t *complete = NULL;

static void mhandler(int sig)
{
	free(find);
	free(complete);
	free(cfinder->nlist);
	free(pfinder);
	free(cfinder);
	exit(EXIT_FAILURE);
}

static void thandler(int sig)
{
	pthread_exit((void *)EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	uint32_t i, j, k;
	uint32_t m;
	uint32_t byte, bit;
	uint32_t nthreads;
	struct sigaction msa;
	int opt;
	
	if (argc < 2)
	{
		printf("Indicate the number of threads.\n");
		exit(EXIT_FAILURE);
	}

	nthreads = atoi(argv[1]);
	find = (sem_t *) malloc(nthreads * sizeof(sem_t));
	complete = (sem_t *) malloc(nthreads * sizeof(sem_t));
	pfinder = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
	cfinder = (struct compFinder *) malloc(sizeof(struct compFinder));
	bzero(cfinder, sizeof(struct compFinder));
	cfinder->nlist = (unsigned char *) malloc(BYTES);
	if (cfinder->nlist == NULL)
	{
		free(pfinder);
		free(cfinder);
		free(complete);
		free(find);
		exit(EXIT_FAILURE);
	}

	/* install signal handler */
	sigemptyset(&msa.sa_mask);
	msa.sa_flags = 0;
	msa.sa_handler = mhandler;
	if (sigaction(SIGINT, &msa, NULL) == -1)
	{
		perror("sigaction");
		free(pfinder);
		free(cfinder->nlist);
		free(cfinder);
		free(complete);
		free(find);
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
/*	for (i = 4; i < (uint32_t)cfinder->total; i += 2)
	{
		byte = i / 8;
		bit = i % 8;
		cfinder->nlist[byte] = cfinder->nlist[byte] | cfinder->mask[bit];
	}*/

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
		i = ++j; 
	}

	cfinder->done = 1;

	/* telling threads to terminate  out of finding primes*/
	for(k = 0; k < nthreads; k++)
		sem_post(&find[k]);

	for (k = 0; k < nthreads; k++)
		sem_wait(&complete[k]);

	/* check what to do next */
	while ((opt = getopt(argc, argv, "phs")) == 'p')
	{
		printBitMap(cfinder, 1, cfinder->total);
	}

	/* set the proper flags */
	while (opt != -1)
	{
		if (opt == 'h')
		{
			printf("happy primes will be found\n");
			cfinder->happy = 1;
			if (getopt(argc, argv, "phs") == 'p')
			{
				printf("happy primes will be printed \n");
				cfinder->phappy = 1;
			}
		}
		else if (opt == 's')
		{
			printf("sad primes will be found\n");
			cfinder->sad = 1;
			if (getopt(argc, argv, "phs") == 'p')
			{
				printf("sad primes will be printed\n");
				cfinder->psad = 1;
			}
		}
		else
		{
			printf("we're done \n");
		}
		opt = getopt(argc, argv, "phs"); 
	}

	/* start up threads again after setting flags */
	for(k = 0; k < nthreads; k++)
		sem_post(&find[k]);

	for (k = 0; k < nthreads; k++)
		pthread_join(pfinder[k], NULL); 

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

void printBitMap(struct compFinder *cfinder,  uint32_t start, uint32_t end)
{
	uint32_t i;
	uint32_t byte;
	uint32_t bit;
	printf("Primes from %u to %u:\n", start, end);
	for (i = start; i <= end; i++)
	{
		byte = i / 8;
		bit = i % 8;
		if (!(cfinder->nlist[byte] & cfinder->mask[bit]))
			printf("%d\n", i);
	}
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
	uint32_t temp;
	uint32_t i;
	uint32_t byte, bit;
	uint32_t start, end;
	uint32_t prime;
//	struct sigaction tsa;

	/* handle interrupt signal */
/*	sigemptyset(&tsa.sa_mask);
	tsa.sa_flags = 0;
	tsa.sa_handler = thandler;
	if (sigaction(SIGINT, &tsa, NULL) == -1)
	{
		perror("sigaction");
		pthread_exit((void *)EXIT_FAILURE);
	}
*/

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
			if (!(i % 2) && prime != 2)
			{
				if ( (compstruct->total - i) < prime)
					break;
				continue;
			}
			byte = i / 8;
			bit = i % 8;
			compstruct->nlist[byte] = compstruct->nlist[byte] | compstruct->mask[bit];
			if ( (compstruct->total - i) < prime)
				break;
		}
		sem_post(&compstruct->complete[temp]);
	}
	sem_post(&compstruct->complete[temp]);

	/* happy/sad determination */
	sem_wait(&compstruct->find[temp]);
	if (compstruct->happy | compstruct->sad)
	{
		/* divide up list of numbers */
		struct bag *hcheck = (struct bag *) malloc(sizeof(struct bag));
		char buf[33];
		if (!temp)
			start = 0;
		else
			start = temp * (BYTES / compstruct->nthreads);
		if (temp == (compstruct->nthreads - 1))
			end = BYTES - 1;
		else 
			end = (temp + 1) * (BYTES / compstruct->nthreads) - 1;

		printf("thread no: %d, start: %u, end: %u\n", temp, start, end);
		/* iterate through prime numbers in region */
		for (byte = start; byte <= end; byte++)
		{
			for (bit = 0; bit < 8; bit++)
			{
				i = 8 * byte + bit;
				/* algorithm similar to that shown in 
				   wikipedia page on happy/sad primes */
				if (!(compstruct->nlist[byte] & compstruct->mask[bit]))
				{
					prime = i;
					initBag(hcheck);
					addToBag(hcheck, 0);
					while ((i > 1) && (!bagContains(hcheck,i)))
					{
						addToBag(hcheck, i);
						sprintf(&buf[0], "%u", i);
						i = calcInterm(&buf[0]);

					}
					if (i == 1 && compstruct->phappy)
					{
						printf("happy: %u\n", prime);
					}

					else if (i != 1 && compstruct->psad)
					{
						if (i)	/* i != 0 */
							printf("sad: %u\n", prime);
					}
					else {
					}

					freeBag(hcheck);	
				}
			}
		}
		free(hcheck);
	}
	return 0;
}

uint32_t calcInterm(char *buf)
{
	int i;
	int len;
	char tempbuf[2];
	uint32_t count = 0;
	uint32_t temp = 0;

	tempbuf[1] = '\0';
	len = strlen(buf);
	for (i = 0; i < len; i++)
	{
		tempbuf[0] = buf[i];
		temp = (uint32_t)atol(tempbuf);
		temp = temp * temp;
		count += temp;
	}
	return count;
}
