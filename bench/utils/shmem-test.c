#include <stdio.h>
#include <assert.h>
#include <omp.h>
#include <sched_comm.h>

#ifndef NUM_THREADS
#define NUM_THREADS 2
#endif

int main(int argc, char** argv)
{
	int i;
	omp_numa_t* ipc_handle = omp_numa_initialize(0);

	printf("Attempting to put 8 tasks on node 0...");
	exec_spec_t setup;
	setup.num_tasks = 8;
	setup.task_assignment[0] = 8;
	for(i = 1; i < omp_numa_num_nodes(ipc_handle); i++)
		setup.task_assignment[i] = 0;
	omp_numa_schedule_tasks(ipc_handle, &setup, 0);
	setup.task_assignment[0] = 0;
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	assert((setup.task_assignment[0] == 8) &&
		omp_numa_num_tasks(ipc_handle, 0, FAST_CHECK) == 8);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	assert(setup.task_assignment[0] == 0);
	printf("success!\n");

	printf("Attempting to put a task on all %d nodes...",
		omp_numa_num_nodes(ipc_handle));
	setup.num_tasks = omp_numa_num_nodes(ipc_handle);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		setup.task_assignment[i] = 1;
	omp_numa_schedule_tasks(ipc_handle, &setup, 0);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		setup.task_assignment[i] = 0;
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		assert(setup.task_assignment[i] == 1);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		assert(setup.task_assignment[i] == 0);
	printf("success!\n");

	printf("Checking multi-threaded correctness...");
	omp_set_num_threads(NUM_THREADS);
#pragma omp parallel
	{
		int j, nodes_per_thread = omp_numa_num_nodes(ipc_handle) / NUM_THREADS;
		exec_spec_t my_setup, cur_setup;

		my_setup.num_tasks = nodes_per_thread;
		cur_setup.num_tasks = 0;

		for(j = 0; j < omp_numa_num_nodes(ipc_handle); j++)
		{
			if((omp_get_thread_num() * nodes_per_thread) <= j &&
					j < ((omp_get_thread_num() + 1) * nodes_per_thread))
				my_setup.task_assignment[j] = 1;
			else
				my_setup.task_assignment[j] = 0;
			cur_setup.task_assignment[j] = 0;
		}

		omp_numa_schedule_tasks(ipc_handle, &my_setup, 0);
#pragma omp barrier
		omp_numa_task_assignment(ipc_handle, cur_setup.task_assignment, 8, 0);
		for(j = 0; j < omp_numa_num_nodes(ipc_handle); j++)
			assert(cur_setup.task_assignment[j] == 1);
#pragma omp barrier
		omp_numa_cleanup(ipc_handle, &my_setup);
#pragma omp barrier
		omp_numa_task_assignment(ipc_handle, cur_setup.task_assignment, 8, 0);
		for(j = 0; j < omp_numa_num_nodes(ipc_handle); j++)
			assert(cur_setup.task_assignment[j] == 0);
	}
	printf("success!\n");

	omp_numa_shutdown(ipc_handle, 0);

	return 0;
}
