#include <stdio.h>
#include <assert.h>
#include <omp.h>
#include "numa_ctl.h"

#define KB (1024L)
#define MB (KB*KB)
#define GB (KB*KB*KB)

int main(int argc, char** argv)
{
	assert(numa_available() > -1 && "NUMA support is not available on this system");

	/* Kernel-dependent values */
	printf("Maximum possible number of NUMA nodes (kernel-dependent): %d\n",
		numa_num_possible_nodes());
	//TODO max # cpus
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

	/* Spawn threads (one for each node) and print NUMA info */
	omp_set_num_threads(numa_num_configured_nodes());
#pragma omp parallel shared(str)
	{
		/* Migrate threads & print info (prevent overlapping */
		numa_bind_node(omp_get_thread_num(), NUMA_MIGRATE);

		/* Prevent overlapping writes to string & printing */
#pragma omp critical
		{
			numa_task_info(str, sizeof(str));
			printf("Thread [%d]: %s\n", omp_get_thread_num(), str);
		}
	}

	return 0;
}
