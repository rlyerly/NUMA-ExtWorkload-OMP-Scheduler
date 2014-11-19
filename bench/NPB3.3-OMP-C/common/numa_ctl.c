#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "numa_ctl.h"

///////////////////////////////////////////////////////////////////////////////
// Initialize & teardown
///////////////////////////////////////////////////////////////////////////////

int numa_initialize()
{
	// Ensure NUMA availability
	assert(numa_available() > -1 &&
		"NUMA support is not available on this system");

	// Assert that no more than one configuration was requested
	int configs = 0;
	if(getenv(ENV_BIND_TO_NODE)) configs++;
	if(getenv(ENV_CPU_NODE)) configs++;
	if(getenv(ENV_MEM_NODE)) configs++;
	assert(configs < 2 && "Please set only one of "
		ENV_BIND_TO_NODE ", " ENV_CPU_NODE ", " ENV_MEM_NODE);

	// Apply requested configuration
	if(getenv(ENV_BIND_TO_NODE))
		return numa_bind_node(atoi(getenv(ENV_BIND_TO_NODE)), NUMA_MIGRATE);
	else if(getenv(ENV_CPU_NODE))
		return numa_run_on_node(atoi(getenv(ENV_CPU_NODE)));
	else if(getenv(ENV_MEM_NODE))
		return numa_set_membind_node(atoi(getenv(ENV_MEM_NODE)), NUMA_MIGRATE);

	return 0;
}

void numa_shutdown()
{
	// Nothing for now
}

///////////////////////////////////////////////////////////////////////////////
// Process & memory migration
///////////////////////////////////////////////////////////////////////////////

int numa_bind_node(unsigned int node, numa_flag_t migrate)
{
	// TODO assert that we can execute on node
	assert(numa_bitmask_isbitset(numa_get_mems_allowed(), node) &&
		"Cannot migrate memory to requested node, not allowed");

	int ret = 0;

	struct bitmask* nm = numa_allocate_nodemask();
	numa_bitmask_setbit(nm, node);

	if(migrate == NUMA_MIGRATE)
		ret = numa_migrate_pages(0, numa_get_membind(), nm);
	numa_bind(nm);

	numa_bitmask_free(nm);
	return ret;
}

int numa_set_membind_node(unsigned int node, numa_flag_t migrate)
{
	assert(numa_bitmask_isbitset(numa_get_mems_allowed(), node) &&
		"Cannot migrate memory to requested node, not allowed");

	int ret = 0;

	struct bitmask* nm = numa_allocate_nodemask();
	numa_bitmask_setbit(nm, node);

	if(migrate == NUMA_MIGRATE)
		ret = numa_migrate_pages(0, numa_get_membind(), nm);
	numa_set_membind(nm);

	numa_free_nodemask(nm);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Convenience functions
///////////////////////////////////////////////////////////////////////////////

void numa_task_info(char* str, size_t str_size)
{
	char tmp[STR_BUF_SIZE];
	struct bitmask* nm = numa_get_run_node_mask();
	struct bitmask* cm = numa_allocate_cpumask();
	str[0] = '\0';
	unsigned int i;

	/* Write the following information:
	 *  1. NUMA node
	 *  2. CPU set
	 */
	numa_nodemask_to_str(nm, tmp, sizeof(tmp));
	snprintf(str, str_size, "Node(s): %s -> CPU(s): ", tmp);

	for(i = 0; i < numa_num_possible_nodes(); i++)
	{
		if(numa_bitmask_isbitset(nm, i))
		{
			numa_node_to_cpus(i, cm);
			numa_cpumask_to_str(cm, tmp, sizeof(tmp));
			strcat(str, tmp);
		}
	}
	str[strlen(str)] = '\0';

	numa_bitmask_free(nm);
	numa_bitmask_free(cm);
}

void numa_nodemask_to_str(struct bitmask* nodes, char* str, size_t str_size)
{
	unsigned int i = 0;
	int first_set = 0;

	char cpu_num[8];

	str[0] = '\0';

	for(i = 0; i < numa_num_possible_nodes(); i++)
	{
		if(numa_bitmask_isbitset(nodes, i))
		{
			if(!first_set)
			{
				snprintf(str, str_size, "%u", i);
				first_set = 1;
			}
			else
			{
				snprintf(cpu_num, sizeof(cpu_num), ",%u", i);
				strcat(str, cpu_num);
			}
		}
	}
}

void numa_cpumask_to_str(struct bitmask* cpus, char* str, size_t str_size)
{
	unsigned int i = 0;
	int first_set = 0;
	char cpu_num[8];

	str[0] = '\0';

	for(i = 0; i < numa_num_possible_cpus(); i++)
	{
		if(numa_bitmask_isbitset(cpus, i))
		{
			if(!first_set)
			{
				snprintf(str, str_size, "%u", i);
				first_set = 1;
			}
			else
			{
				snprintf(cpu_num, sizeof(cpu_num), ",%u", i);
				strcat(str, cpu_num);
			}
		}
	}
}

