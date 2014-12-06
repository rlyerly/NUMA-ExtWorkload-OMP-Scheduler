#include <stdio.h>
#include <math.h>
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

	// Sanity check - put tasks on a node
	printf("Attempting to put 8 tasks on node 0...");
	exec_spec_t setup;
	setup.num_tasks = 8;
	setup.task_assignment[0] = 8;
	for(i = 1; i < omp_numa_num_nodes(); i++)
		setup.task_assignment[i] = 0;
	omp_numa_map_tasks(ipc_handle, &setup, 0);
	setup.task_assignment[0] = 0;
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	assert((setup.task_assignment[0] == 8) &&
		omp_numa_num_tasks(ipc_handle, 0, FAST_CHECK) == 8);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	assert(setup.task_assignment[0] == 0);
	printf("success!\n");

	// Sanity check - put tasks on all nodes
	printf("Attempting to put a task on all %d nodes...",
		omp_numa_num_nodes());
	setup.num_tasks = omp_numa_num_nodes();
	for(i = 0; i < omp_numa_num_nodes(); i++)
		setup.task_assignment[i] = 1;
	omp_numa_map_tasks(ipc_handle, &setup, 0);
	for(i = 0; i < omp_numa_num_nodes(); i++)
		setup.task_assignment[i] = 0;
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	for(i = 0; i < omp_numa_num_nodes(); i++)
		assert(setup.task_assignment[i] == 1);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 8, 0);
	for(i = 0; i < omp_numa_num_nodes(); i++)
		assert(setup.task_assignment[i] == 0);
	printf("success!\n");

	// Rob: this code only works when OpenMP/NUMA control is NOT integrated into
	//      the OpenMP library!
/*	printf("Checking multi-threaded correctness...");
	omp_set_num_threads(NUM_THREADS);
#pragma omp parallel
	{
		int j, nodes_per_thread = omp_numa_num_nodes() / NUM_THREADS;
		exec_spec_t my_setup, cur_setup;

		my_setup.num_tasks = nodes_per_thread;
		cur_setup.num_tasks = 0;

		for(j = 0; j < omp_numa_num_nodes(); j++)
		{
			if((omp_get_thread_num() * nodes_per_thread) <= j &&
					j < ((omp_get_thread_num() + 1) * nodes_per_thread))
				my_setup.task_assignment[j] = 1;
			else
				my_setup.task_assignment[j] = 0;
			cur_setup.task_assignment[j] = 0;
		}

		omp_numa_map_tasks(ipc_handle, &my_setup, 0);
#pragma omp barrier
		omp_numa_task_assignment(ipc_handle, cur_setup.task_assignment, 8, 0);
		for(j = 0; j < omp_numa_num_nodes(); j++)
			assert(cur_setup.task_assignment[j] == 1);
#pragma omp barrier
		omp_numa_cleanup(ipc_handle, &my_setup);
#pragma omp barrier
		omp_numa_task_assignment(ipc_handle, cur_setup.task_assignment, 8, 0);
		for(j = 0; j < omp_numa_num_nodes(); j++)
			assert(cur_setup.task_assignment[j] == 0);
	}
	printf("success!\n");*/

	// This assumes that # processors is equally divisible by 4
	printf("Checking thread mapping algorithm correctness...");
	exec_spec_t* setup1, *setup2, *setup3, *setup4;

	setup1 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	assert(setup1->num_tasks == omp_numa_num_procs());
	for(i = 0; i < omp_numa_num_nodes(); i++)
		assert(setup1->task_assignment[i] == omp_numa_num_procs_per_node());

	setup2 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	assert(setup2->num_tasks == (omp_numa_num_procs() / 2));
	for(i = 0; i < omp_numa_num_nodes(); i++)
	{
		if(i < (omp_numa_num_nodes() / 2))
			assert(setup2->task_assignment[i] == omp_numa_num_procs_per_node());
		else
			assert(setup2->task_assignment[i] == 0);
	}

	omp_numa_cleanup(ipc_handle, setup1);
	free(setup1);
	setup1 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	assert(setup1->num_tasks == (omp_numa_num_procs() / 2));
	for(i = 0; i < omp_numa_num_nodes(); i++)
	{
		if(i < (omp_numa_num_nodes() / 2))
			assert(setup1->task_assignment[i] == 0);
		else
			assert(setup1->task_assignment[i] == omp_numa_num_procs_per_node());
	}

	omp_numa_cleanup(ipc_handle, setup1);
	omp_numa_cleanup(ipc_handle, setup2);
	free(setup1);
	free(setup2);

	setup1 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	setup2 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	setup3 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	assert(setup3->num_tasks ==
		(unsigned)ceil((double)omp_numa_num_procs() / 3.0));
	unsigned num_remaining = setup3->num_tasks;
	for(i = 0; i < omp_numa_num_nodes(); i++)
	{
		if(i < omp_numa_num_nodes() / 2)
			assert(setup3->task_assignment[i] == 0);
		else
		{
			unsigned chunk = (omp_numa_num_procs_per_node() > num_remaining ?
				num_remaining : omp_numa_num_procs_per_node());
			assert(setup3->task_assignment[i] == chunk);
			num_remaining -= chunk;
		}
	}

	setup4 = omp_numa_map_tasks(ipc_handle, NULL, 0);
	assert(setup4->num_tasks == omp_numa_num_procs() / 4);
	num_remaining = setup4->num_tasks;
	for(i = 0; i < omp_numa_num_nodes(); i++)
	{
		//TODO check this is correct -- for now, just print & check manually
		printf("[%d] %d\n", i, setup4->task_assignment[i]);
	}

	omp_numa_cleanup(ipc_handle, setup1);
	omp_numa_cleanup(ipc_handle, setup2);
	omp_numa_cleanup(ipc_handle, setup3);
	omp_numa_cleanup(ipc_handle, setup4);
	free(setup1);
	free(setup2);
	free(setup3);
	free(setup4);

	printf("success!\n");

	omp_numa_shutdown(ipc_handle, 0);
	return 0;
}

