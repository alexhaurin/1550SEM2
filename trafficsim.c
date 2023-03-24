#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MMAP(s) mmap(NULL, s, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
#define MAX_CAR_COUNT 12

struct ListNode
{
	struct ListNode* next;
	struct task_struct* currentTask;
};

struct cs1550_sem
{
	struct ListNode* head;
	int value;
};

void up(struct cs1550_sem* s) { syscall(442, s); }
void down(struct cs1550_sem* s) { syscall(411, s); }

struct cs1550_sem* pNorthMutex;
struct cs1550_sem* pNorthEmpty;
struct cs1550_sem* pNorthFull;
struct cs1550_sem* pSouthMutex;
struct cs1550_sem* pSouthEmpty;
struct cs1550_sem* pSouthFull;
struct cs1550_sem* pTotalCount;
int* pNorthIndex;
int* pNorthCars;
int* pSouthIndex;
int* pSouthCars;


int main(char argc, char** argv)
{
	srand(time(NULL));

	// Allocate are sems and variables
	pNorthMutex = MMAP(sizeof(struct cs1550_sem));
	pNorthMutex->value = 1;
	pNorthEmpty = MMAP(sizeof(struct cs1550_sem));
	pNorthEmpty->value = MAX_CAR_COUNT;
	pNorthFull = MMAP(sizeof(struct cs1550_sem));
	pNorthFull->value = 0;

	pSouthMutex = MMAP(sizeof(struct cs1550_sem));
	pSouthMutex->value = 1;
	pSouthEmpty = MMAP(sizeof(struct cs1550_sem));
	pSouthEmpty->value = MAX_CAR_COUNT;
	pSouthFull = MMAP(sizeof(struct cs1550_sem));
	pSouthFull->value = 0;

	pTotalCount = MMAP(sizeof(struct cs1550_sem));
	pTotalCount->value = 0;

	pSouthCars = MMAP(sizeof(int)*MAX_CAR_COUNT);
	pNorthCars = MMAP(sizeof(int)*MAX_CAR_COUNT);

	// Run our child processes
	// North
	pid_t northID = fork();
	if (northID == 0) // Were in the child
	{
		int index = 0;
		while (1)
		{
			down(pNorthEmpty);
			down(pNorthMutex);

			printf("Car number %d has arrived from the north at time %d", index, time(NULL));
			// If we need to honk
			if (pTotalCount->value <= 0)
			{
				printf(" and they blew their horn!\n");
			}
			else { printf("\n"); }

			// Add our first car
			pNorthCars[index] = index;
			index = (index + 1) % MAX_CAR_COUNT;


			// 70% for each next car
			while(rand()%10 < 7)
			{
				//printf("Random\n");
				printf("Car number %d has arrived from the north at time %d\n", index, time(NULL));
				pNorthCars[index] = index;
				index = (index + 1) % MAX_CAR_COUNT;
				up(pNorthFull);
				up(pTotalCount);
			}

			up(pNorthMutex);
			up(pNorthFull);
			up(pTotalCount);

			// Takes 10 seconds for next burst
			sleep(10);
		}
	}

	// South
	pid_t southID = fork();
	if (southID == 0) // Were in the child
	{
		int index = 0;
		while (1)
		{
			down(pSouthEmpty);
			down(pSouthMutex);

			printf("Car number %d has arrived from the south at time %d", index, time(NULL));
			// If we need to honk
			if (pTotalCount->value <= 0)
			{
				printf(" and the blew their horn!\n");
			}
			else { printf("\n"); }

			// Add our first car
			pSouthCars[index] = index;
			index = (index + 1) % MAX_CAR_COUNT;

			// 70% for each next car
			while(rand()%10 < 7)
			{
				printf("Car number %d has arrived from the south at time %d\n", index, time(NULL));
				pSouthCars[index] = index;
				index = (index + 1) % MAX_CAR_COUNT;
				up(pSouthFull);
				up(pTotalCount);
			}

			up(pSouthMutex);
			up(pSouthFull);
			up(pTotalCount);

			// Takes 10 seconds for next burst
			sleep(10);
		}
	}

	// Consumer/Flag person
	pid_t cID = fork();
	if (cID == 0)
	{
		int outNorth = 0;
		int outSouth = 0;
		while (1)
		{
			// Wait and fall asleep if there are no more cars
			if (pTotalCount->value <= 0)
			{
				printf("The flag person fell asleep!\n");
				down(pTotalCount);
				printf("The flag person woke up!\n");
			}

			// North consumption
			while (1)
			{
				down(pNorthFull);
				down(pNorthMutex);
				down(pTotalCount);

				// Remove the car
				printf("Car number %d is leaving from the north at time %d\n", pNorthCars[outNorth], time(NULL));
				outNorth = (outNorth + 1) % MAX_CAR_COUNT;

				up(pNorthMutex);
				up(pNorthEmpty);

				// Takes 1 sec for the car to pass
				sleep(1);

				// Move on the the other direction if more than 12 cars there or none here
				if (pSouthFull->value >= 12) { break; }
				if (pNorthFull->value <= 0) { break; }
			}

			// South consumption
			while (1)
			{
				down(pSouthFull);
				down(pSouthMutex);
				down(pTotalCount);

				// Remove the car
				printf("Car number %d is leaving from the south at time %d\n", pSouthCars[outSouth], time(NULL));
				outSouth = (outSouth + 1) % MAX_CAR_COUNT;

				up(pSouthMutex);
				up(pSouthEmpty);

				// Takes 1 sec for the car to pass
				sleep(1);

				// Move on the the other direction if more than 12 cars there or none here
				if (pNorthFull->value >= 12) { break; }
				if (pSouthFull->value <= 0) { break; }
			}
		}
	}

	// wait for the child processes to finish
	wait(NULL);

	kill(northID, SIGKILL);
	kill(southID, SIGKILL);
	kill(northID, SIGKILL);

	// Return sucess
	return 0;
}
