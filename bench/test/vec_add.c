#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <omp.h>
#include <numa_ctl.h>

#define NUM_ELEMS 1024 //(1024 * 1024)
#define ARR_SIZE (sizeof(int) * NUM_ELEMS)

///////////////////////////////////////////////////////////////////////////////
// Parallel code
///////////////////////////////////////////////////////////////////////////////

void vector_sum(const int* a, const int* b, int* c, size_t num_elems)
{
	int i, j;
	for(j = 0; j < 2; j++)
	{
#pragma omp parallel 
		{
#pragma omp critical
			{
				char task_info[4096];
				numa_task_info(task_info, sizeof(task_info));
				printf("[%d] %s\n", omp_get_thread_num(), task_info);
			}

#pragma omp for
			for(i = 0; i < num_elems; i++)
				c[i] = a[i] + b[i];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Setup code
///////////////////////////////////////////////////////////////////////////////

void randomize(int* vec, int num_elems)
{
	int i;
	for(i = 0; i < num_elems; i++)
		vec[i] = rand();
}

void zero(int* vec, int num_elems)
{
	int i;
	for(i = 0; i < num_elems; i++)
		vec[i] = 0;
}

int main(int argc, char** argv)
{
	printf("Setting up memory...");

	int i;
	int* a = (int*)malloc(ARR_SIZE);
	int* b = (int*)malloc(ARR_SIZE);
	int* c = (int*)malloc(ARR_SIZE);

	assert(a != NULL && b != NULL && c != NULL);

	randomize(a, NUM_ELEMS);
	randomize(b, NUM_ELEMS);
	zero(c, NUM_ELEMS);

	printf("success!\nRunning threaded vector sum...");

	vector_sum(a, b, c, NUM_ELEMS);

	printf("success!\nCleaning up & sanity check...");

	for(i = 0; i < NUM_ELEMS; i++)
		assert(c[i] == (a[i] + b[i]));

	free(a);
	free(b);
	free(c);

	printf("success!\n");

	return 0;
}

