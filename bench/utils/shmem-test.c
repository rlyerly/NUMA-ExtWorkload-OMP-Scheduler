#include <stdio.h>
#include <assert.h>
#include <sched_comm.h>

int main(int argc, char** argv)
{
	int i;
	omp_numa_t* ipc_handle = omp_numa_initialize(0);

	printf("Attempting to put 8 tasks on node 0...");
	exec_spec_t setup;
	setup.num_tasks = 8;
	setup.task_assignment[0] = 8;
	omp_numa_schedule_tasks(ipc_handle, &setup, 0);
	setup.task_assignment[0] = 0;
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 64, 0);
	assert((setup.task_assignment[0] == 8) &&
		omp_numa_num_tasks(ipc_handle, 0, FAST_CHECK) == 8);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 64, 0);
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
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 64, 0);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		assert(setup.task_assignment[i] == 1);
	printf("success!\nAttempting to clean up...");
	omp_numa_cleanup(ipc_handle, &setup);
	omp_numa_task_assignment(ipc_handle, setup.task_assignment, 64, 0);
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
		assert(setup.task_assignment[i] == 0);
	printf("success!\n");

	omp_numa_shutdown(ipc_handle, 0);

	return 0;
}
