#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* For system scheduling API */
#define __USE_GNU
#include <sched.h>

#include "numa_ctl.h"

///////////////////////////////////////////////////////////////////////////////
// Initialize & teardown
///////////////////////////////////////////////////////////////////////////////

int numa_initialize(numa_node_t mem_node,
										numa_node_t exec_node,
										numa_flag_t flags)
{
	// Ensure NUMA availability
	assert(numa_available() > -1 &&
		"NUMA support is not available on this system");

	//TODO save current NUMA configuration

	// Assert that no more than one configuration was requested
	int configs = 0;
	if(getenv(NUMA_BIND_TO_NODE)) configs++;
	if(getenv(NUMA_CPU_NODE) || getenv(NUMA_MEM_NODE)) configs++;
	assert(configs < 2 && "Please set either "
		NUMA_BIND_TO_NODE " or " NUMA_CPU_NODE " + " NUMA_MEM_NODE);

	if(mem_node != CURRENT_NODE || exec_node != CURRENT_NODE) // apply requested
	{																													// behavior or...
		if(mem_node == exec_node)
			return numa_bind_node(mem_node, flags);
		else if(exec_node != CURRENT_NODE)
			return numa_run_on_node(exec_node);
		else
			return numa_set_membind_node(mem_node, flags);
	}
	else if(NUMA_ENV_CONFIG(flags)) // ...get NUMA behavior from environment
	{
		if(getenv(NUMA_BIND_TO_NODE))
			return numa_bind_node(atoi(getenv(NUMA_BIND_TO_NODE)), NUMA_MIGRATE);
		else if(getenv(NUMA_CPU_NODE))
			return numa_run_on_node(atoi(getenv(NUMA_CPU_NODE)));
		else if(getenv(NUMA_MEM_NODE))
			return numa_set_membind_node(atoi(getenv(NUMA_MEM_NODE)), NUMA_MIGRATE);
	}

	return 0;
}

void numa_shutdown()
{
	//TODO restore previous NUMA configuration, if one exists
}

///////////////////////////////////////////////////////////////////////////////
// Process & memory migration
///////////////////////////////////////////////////////////////////////////////

int numa_bind_node(numa_node_t node, numa_flag_t flags)
{
	// TODO assert that we can execute on node
	assert(numa_bitmask_isbitset(numa_get_mems_allowed(), node) &&
		"Cannot migrate memory to requested node, not allowed");

	int ret = 0;

	struct bitmask* nm = numa_allocate_nodemask();
	numa_bitmask_setbit(nm, node);

	if(NUMA_DO_MIGRATE( flags ))
		ret = numa_migrate_pages(0, numa_get_membind(), nm);
	numa_bind(nm);

	numa_bitmask_free(nm);
	return ret;
}

int numa_set_membind_node(numa_node_t node, numa_flag_t flags)
{
	assert(numa_bitmask_isbitset(numa_get_mems_allowed(), node) &&
		"Cannot migrate memory to requested node, not allowed");

	int ret = 0;

	struct bitmask* nm = numa_allocate_nodemask();
	numa_bitmask_setbit(nm, node);

	if(NUMA_DO_MIGRATE( flags ))
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

	/*  1. NUMA node */
	numa_nodemask_to_str(nm, tmp, sizeof(tmp));
	snprintf(str, str_size, "Node(s): %s -> CPU(s): ", tmp);

	/* 2. CPU set */
	numa_nodemask_to_cpumask(nm, cm);
	numa_cpumask_to_str(cm, tmp, sizeof(tmp));
	strcat(str, tmp);

	/*  3. Current CPU of execution */
	snprintf(tmp, sizeof(tmp), ", executing on CPU %d", sched_getcpu());
	strcat(str, tmp);

	numa_bitmask_free(nm);
	numa_bitmask_free(cm);
}

void numa_nodemask_to_cpumask(const struct bitmask* nodes, struct bitmask* cpus)
{
	struct bitmask* temp_cpus = numa_allocate_cpumask(); 
	numa_node_t i, j;

	for(i = 0; i < numa_num_possible_nodes(); i++)
	{
		if(numa_bitmask_isbitset(nodes, i))
		{
			numa_node_to_cpus(i, temp_cpus);
			for(j = 0; j < numa_num_possible_cpus(); j++)
				if(numa_bitmask_isbitset(temp_cpus, j))
					numa_bitmask_setbit(cpus, j);
		}
	}

	numa_bitmask_free(temp_cpus);
}

void numa_nodemask_to_str(const struct bitmask* nodes, char* str, size_t str_size)
{
	numa_node_t i = 0;
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

void numa_cpumask_to_str(const struct bitmask* cpus, char* str, size_t str_size)
{
	numa_node_t i = 0;
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

