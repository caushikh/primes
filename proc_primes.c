#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <strings.h>
#include <semaphore.h>
#include <signal.h>
#include "listbag.h"

#define BYTES 536870912
#define TOTAL BYTES*8-1
#define MAX_PROC 8

struct compFinder {
	unsigned char *nlist;
	unsigned char mask[8];
	uint32_t prime;
	uint32_t procNo;
	uint32_t nproc;
	uint32_t total;
	int done;
	sem_t find[MAX_PROC];
	sem_t complete[MAX_PROC];
	int happy;
	int sad;
	int phappy;
	int psad;
};

void printBitMap(struct compFinder *, uint32_t, uint32_t );
int getNumPrimes(struct compFinder *cfinder);
uint32_t calcInterm(char *buf);

static struct compFinder *cfinder = NULL;
static sem_t *find = NULL;
static sem_t *complete = NULL;
static sem_t *getid = NULL; 
static void *pstruct = NULL;
static char *path;
static uint32_t nproc = 1;

static void mhandler(int sig)
{
	int j;
	for (j = 0; j < cfinder->nproc; j++)
	{
		sem_destroy(&find[j]);
		sem_destroy(&complete[j]);
	}
	sem_destroy(getid);

	for (j = 0; j < nproc; j++)
		wait(NULL);

	if (shm_unlink(path) == -1)
	{
		perror("unlink shared memory\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_FAILURE);
}

static void chandler(int sig)
{
	exit(EXIT_FAILURE);
}

/* taken from Kevin Mcgrath's lecture on shared memory */
void *mount_shmem(char *path, uint64_t object_size)
{
	int shmem_fd;
	void *addr;

	shmem_fd = shm_open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (shmem_fd == -1)
	{
		printf("failed to open shared memory object.\n");
		exit(EXIT_FAILURE);
	}

	if (ftruncate(shmem_fd, object_size) == -1)
	{
		printf("failed to resize shared memory object.\n");
		exit(EXIT_FAILURE);
	}

	addr = mmap(NULL, object_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		shmem_fd, 0);

	if (addr == MAP_FAILED)
	{
		printf("failed to map shared memory object.\n");
		exit(EXIT_FAILURE);
	}

	return addr;
}

int main(int argc, char *argv[])
{
	uint32_t i, j, k;
	uint32_t m;
	uint32_t byte, bit;
	struct sigaction msa;
	struct sigaction csa;
	uint64_t object_size =  BYTES + 1024*1024;
	int opt;

	/* child variables */
	uint32_t nmult;
	uint32_t mpert;
	uint32_t temp;
	uint32_t a;
	uint32_t cbyte, cbit;
	uint32_t start, end;
	uint32_t prime;

	if (argc < 2)
	{
		printf("Indicate the number of threads.\n");
		exit(EXIT_FAILURE);
	}
	
	/* map shared memory object, allocate into it */
	nproc = atoi(argv[1]);
	if (nproc > MAX_PROC)
	{
		printf("The process limit is 8.\n");
		exit(EXIT_FAILURE);
	}
	path = "/hari_primes";
	pstruct = mount_shmem(path, object_size);
	cfinder = (struct compFinder *) pstruct;
	bzero(cfinder, sizeof(struct compFinder));

	cfinder->nlist = (unsigned char *) (&cfinder[1]);
	find = &cfinder->find[0];
	complete = &cfinder->complete[0];
	getid = (sem_t *) (&cfinder->nlist[BYTES]);

	/* initialize composite finder struct and number list */
	bzero(cfinder->nlist, BYTES);
	cfinder->nproc = nproc;

	/* initialize semaphores */
	for (i = 0; i < nproc; i++)
	{
		sem_init(&find[i], 1, 0);
		sem_init(&complete[i], 1, 0);
	}

	sem_init(getid, 1, 1);

	/* install signal handler */
	sigemptyset(&msa.sa_mask);
	msa.sa_flags = 0;
	msa.sa_handler = mhandler;
	if (sigaction(SIGINT, &msa, NULL) == -1)
	{
		int j;
		for (j = 0; j < cfinder->nproc; j++)
		{
			sem_destroy(&find[j]);
			sem_destroy(&complete[j]);
		}
		sem_destroy(getid);
	
		for (j = 0; j < nproc; j++)
			wait(NULL);

		if (shm_unlink(path) == -1)
		{
			perror("unlink shared memory\n");
			exit(EXIT_FAILURE);
		}
		
		exit(EXIT_FAILURE);

	}


	/* initialize mask */
	for (i = 0; i < 8; i++)
	{
		cfinder->mask[i] = 1 << (8-i-1);
	}

	cfinder->total = (uint32_t) ((unsigned long)(BYTES) * 8 - 1);

	/* mark the number 1 as special */
	cfinder->nlist[0] = cfinder->nlist[0] | cfinder->mask[1];
	cfinder->done = 0;

	/* create composite marking processes */
	for (i = 0; i < nproc; i++)
	{
		switch(fork())
		{
			case -1:
		
				for (j = 0; j < nproc; j++)
				{
					sem_destroy(&find[j]);
					sem_destroy(&complete[j]);
				}
				sem_destroy(getid);
				if (shm_unlink(path) == -1)
				{
					perror("unlink pstruct\n");
					exit(EXIT_FAILURE);
					perror("forking");
				}
		
				exit(EXIT_FAILURE);
				break;
			case 0: /* child */
				
				/* install child signal handler */
				sigemptyset(&csa.sa_mask);
				csa.sa_flags = 0;
				csa.sa_handler = chandler;
				if (sigaction(SIGINT, &csa, NULL) == -1)
				{
					exit(EXIT_FAILURE);
				}

				/* get process number */
				sem_wait(getid);
				temp = cfinder->procNo;
				cfinder->procNo++;
				sem_post(getid);
				/* wait for signal and get prime */
				while(1)
				{
					sem_wait(&cfinder->find[temp]);
					if (cfinder->done)
						break;
					prime = cfinder->prime;
					nmult = cfinder->total / prime;
					mpert = nmult / cfinder->nproc;
			
	
					if (!mpert)
					{
						/* multiples per thread greater
						   than the number of threads */
						for (a = 0; a < nmult - 1; a++)
						{
							if (temp == a)
							{
								start = (a + 2) * prime;
								end = (a + 2) * prime;
							}
						}
		
						for (a = nmult - 1; a < cfinder->nproc; a++)
						{
							/* for inactive threads
							 just make start
							 arbitrarily greater
							 than end */
							if (temp == a)
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
							/* set start to first
							   multiple in byte */
							while (1)
							{
								if ( (start / 8) != ((start - prime) / 8))
								{
									/* first
									multiple
									in byte
									*/
									break;
								}
								start += prime;
							}
						}	
				
						/* set end composite number */	
						if (temp == (cfinder->nproc - 1))
						{
							end = nmult * prime;
						}
						else
						{
							end = ((temp + 1) * mpert + 1) * prime;
							while(1)
							{
								/* set end to
								 last multiple
								 of byte */
								if ( ((end + prime) / 8) != (end / 8))
								{
									/* last
									multiple
									of byte
									*/
									break;
								}
								end += prime;
							}
						}	
					}
					for (a = start; a <= end; a += prime)
					{
						if (!(a % 2) && prime != 2)
						{
							if ( (cfinder->total - a) < prime)
								break;
							continue;
						}
						cbyte = a / 8;
						cbit = a % 8;
						cfinder->nlist[cbyte] = cfinder->nlist[cbyte] | cfinder->mask[cbit];
						if ( (cfinder->total - a) < prime)
							break;
					}
					sem_post(&cfinder->complete[temp]);
				}
				sem_post(&cfinder->complete[temp]);

				/* happy/sad determination */
				sem_wait(&cfinder->find[temp]);
				if (cfinder->happy | cfinder->sad)
				{
					/* divide up list of numbers */
					struct bag *hcheck = (struct bag *) malloc(sizeof(struct bag));
					char buf[33];
					if (!temp)
						start = 0;
					else
						start = temp * (BYTES / cfinder->nproc);
					if (temp == (cfinder->nproc - 1))
						end = BYTES - 1;
					else 
						end = (temp + 1) * (BYTES / cfinder->nproc) - 1;

					/* iterate through prime numbers in region */
					for (cbyte = start; cbyte <= end; cbyte++)
					{
						for (cbit = 0; cbit < 8; cbit++)
						{
							i = 8 * cbyte + cbit;
						/* algorithm similar to that shown in 
						   wikipedia page on happy/sad primes */
							if (!(cfinder->nlist[cbyte] & cfinder->mask[cbit]))
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
								if (i == 1 && cfinder->phappy)
								{
									printf("happy: %u\n", prime);
								}

								else if (i != 1 && cfinder->psad)
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

				exit(EXIT_SUCCESS);
				break;
			default:
				
				break;
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
		for(k = 0; k < nproc; k++)
			sem_post(&find[k]);
	
		/* waiting for all threads to finish before getting next prime 
		 */
		for (k = 0; k < nproc; k++)
		{
			if (sem_wait(&complete[k])==-1)
				perror("error\n");
		}
		i = ++j; 
	}
	cfinder->done = 1;

	/* telling processes to terminate */
	for(k = 0; k < nproc; k++)
		sem_post(&find[k]);

	for (k = 0; k < nproc; k++)
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

	/* start up processes again after setting flags */
	for(k = 0; k < nproc; k++)
		sem_post(&find[k]);

	for (k = 0; k < nproc; k++)
		wait(NULL); 

	/* destroy semaphores, delete shared memory object */
	for (j = 0; j < nproc; j++)
	{
		sem_destroy(&find[j]);
		sem_destroy(&complete[j]);
	}
	sem_destroy(getid);
	if (shm_unlink(path) == -1)
	{
		perror("unlink pstruct\n");
		exit(EXIT_FAILURE);
	}
	
	return 0;
}

void printBitMap(struct compFinder *cfinder,  uint32_t start, uint32_t end)
{
	uint32_t i;
	uint32_t byte;
	uint32_t bit;
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
