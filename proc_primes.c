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

#define BYTES 2/*536870912*/
#define TOTAL BYTES*8-1

struct compFinder {
	unsigned char *nlist;
	unsigned char mask[8];
	uint32_t prime;
	uint32_t procNo;
	uint32_t nproc;
	uint32_t total;
	int done;
	sem_t *find;
	sem_t *complete;
};

void printBitMap(struct compFinder *, uint32_t, uint32_t );
int getNumPrimes(struct compFinder *cfinder);

static struct compFinder *cfinder = NULL;
static sem_t *find = NULL;
static sem_t *complete = NULL;
static sem_t *getid = NULL; 
static void *pstruct = NULL;
static char *path;

static void mhandler(int sig)
{
	int j;
	for (j = 0; j < cfinder->nproc; j++)
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
	uint32_t nproc;
	struct sigaction msa;
	uint64_t object_size =  BYTES + 1024*1024;

	/* child variables */
	uint32_t nmult;
	uint32_t mpert;
	uint32_t temp;
	uint32_t a;
	uint32_t cbyte, cbit;
	uint32_t start, end;
	uint32_t prime;

	if (argc != 2)
	{
		printf("Indicate the number of threads.\n");
		exit(EXIT_FAILURE);
	}
	
	/* map shared memory object, allocate into it */
	nproc = atoi(argv[1]);
	path = "/hari_primes";
	pstruct = mount_shmem(path, object_size);
	cfinder = (struct compFinder *) pstruct;
	bzero(cfinder, sizeof(struct compFinder));

	cfinder->nlist = (unsigned char *) (&cfinder[1]);
	find = (sem_t *) (&cfinder->nlist[BYTES]);
	complete = (sem_t *) (&find[nproc]);
	getid = (sem_t *) (&complete[nproc]);

	/* initialize composite finder struct and number list */
	bzero(cfinder->nlist, BYTES);
	cfinder->nproc = nproc;

	/* initialize semaphores */
	for (i = 0; i < nproc; i++)
	{
		sem_init(&find[i], 1, 0);
		int a = sem_init(&complete[i], 1, 0);
		a = sem_trywait(&complete[i]);
		printf("sem init: %d\n", a);

	}

	sem_init(getid, 1, 1);
	cfinder->find = find;
	cfinder->complete = complete;
	int val;
	sem_getvalue(&complete[0], &val);
	printf("complete: %d\n", val);
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
		#if 0
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
		#endif
				exit(EXIT_FAILURE);
				break;
			case 0: /* child */
					
				/* get process number */
				sleep(10);
				sem_wait(getid);
				temp = cfinder->procNo;
				cfinder->procNo++;
				sem_post(getid);
				/* wait for signal and get prime */
				while(1)
				{
					sem_wait(&cfinder->find[temp]);
	sem_getvalue(&complete[0], &val);
	printf("child complete: %d\n", val);

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
					printf("Proc no: %u, start: %u, end: %u\n", cfinder->procNo, start, end);
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
		printf("i = %d\n", i);		
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
			sem_post(&cfinder->find[k]);
	
		/* waiting for all threads to finish before getting next prime 
		 */
	sem_getvalue(&complete[0], &val);
	printf("second parent complete: %d, nproc: %u\n", val, nproc);

		for (k = 0; k < nproc; k++)
		{
			printf("trywait: %d\n", sem_trywait(&cfinder->complete[k]));
			if (sem_wait(&cfinder->complete[k])==-1)
				perror("error\n");
		}
		i = ++j; 
	}
	cfinder->done = 1;

	/* telling processes to terminate */
	for(k = 0; k < nproc; k++)
		sem_post(&find[k]);
	
	for (k = 0; k < nproc; k++)
		wait(NULL); 

	/* get number of primes */
	printBitMap(cfinder, 1, 15);
	printf("There are %d primes.\n", getNumPrimes(cfinder));

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

