#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <omp.h>
#include "numa_ctl.h"

/* For sched_getcpu definition */
#define __USE_GNU
#include <sched.h>

#define KB (1024L)
#define MB (KB*KB)
#define GB (KB*KB*KB)

void print_header(const char* msg)
{
	size_t len = strlen(msg) + 5;
	char* marker = (char*)malloc(len);
	size_t i;
	for(i = 0; i < len-1; i++)
		marker[i] = '=';
	marker[len-1] = '\0';

	printf("%s\n= %s =\n%s\n\n", marker, msg, marker);
}

int main(int argc, char** argv)
{
	numa_initialize(numa_all_nodes_ptr, numa_all_nodes_ptr, NO_FLAGS);

	printf("\n"); // Perfectionism...
	print_header("GENERAL NUMA INFO");

	/* Kernel-dependent values */
	printf("Maximum possible number of NUMA nodes (kernel-dependent): %d\n",
		numa_num_possible_nodes());
	printf("Maximum possible number of CPUs (kernel-dependent): %d\n",
		numa_num_possible_cpus());
	printf("Number of NUMA nodes: %d\n", numa_num_configured_nodes());
	printf("Number of CPUs: %d\n\n", numa_num_configured_cpus());

	/* System & task-dependent values */
	char str[256];
	const unsigned int num_nodes = numa_num_configured_nodes();
	unsigned int i = 0;
	long mem = 0, free = 0;
	struct bitmask* bm =
		(struct bitmask*)numa_bitmask_alloc(numa_num_possible_cpus());

	numa_nodemask_to_str(numa_get_mems_allowed(), str, sizeof(str));
	printf("Nodes for which this CPU can allocate memory: %s\n", str);
	printf("Preferred node for this CPU: %d\n", numa_preferred());
	printf("Size of NUMA pages: %ldKB\n\n", numa_pagesize()/KB);

	/* Per-node values */
	for(i = 0; i < num_nodes; i++)
	{
		printf("Node %d:\n", i);
		mem = numa_node_size(i, &free);
		printf("\tSize of node: %ldGB (%ldMB), %ldGB free (%ldMB)\n",
			mem/GB, mem/MB, free/GB, free/MB);
		numa_node_to_cpus(i, bm);
		numa_cpumask_to_str(bm, str, sizeof(str));
		printf("\tCPUs: %s\n\n", str);
	}

	numa_bitmask_free(bm);

	print_header("OPENMP THREAD BEHAVIOR");

	/* See if child processes inherit NUMA nodes/CPUs */
	omp_set_num_threads(numa_num_configured_nodes());
	numa_initialize(0, 0, NUMA_MIGRATE_EXISTING);
	struct bitmask* parent_nm = numa_get_run_node_mask();
	printf("Set parent node to 0, checking that children inherit...\n\n");

#pragma omp parallel shared(parent_nm)
	{
#pragma omp critical
		{
			numa_task_info(str, sizeof(str));
			printf("Thread [%d]: %s\n", omp_get_thread_num(), str);

			struct bitmask* nm = numa_get_run_node_mask();
			assert(numa_bitmask_equal(parent_nm, nm) && "Child mask doesn't match parent mask!");
			numa_free_cpumask(nm);
		}
	}
	numa_free_cpumask(parent_nm);

	/* Migrate threads & print/verify NUMA info */
	printf("\nMigrating threads...\n\n");
#pragma omp parallel shared(str)
	{
		numa_bind_node(omp_get_thread_num(), NUMA_MIGRATE_EXISTING);

		/* Sanity check migration & print thread info */
#pragma omp critical
		{
			numa_task_info(str, sizeof(str));
			printf("Thread [%d]: %s\n", omp_get_thread_num(), str);

			struct bitmask* nm = numa_get_run_node_mask();
			struct bitmask* cm = numa_allocate_cpumask();
			numa_nodemask_to_cpumask(nm, cm);
			int cpu = sched_getcpu();
			assert(numa_bitmask_isbitset(cm, cpu) && "Task is not executing on correct node!");
			numa_free_cpumask(cm);
		}
	}
	printf("\n");

	return 0;
}

