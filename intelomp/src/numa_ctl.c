#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* For system scheduling API */
#include <sched.h>

#include "numa_ctl.h"

#define _VERBOSE_NUMA // <-- TODO remove

///////////////////////////////////////////////////////////////////////////////
// Internal functions
///////////////////////////////////////////////////////////////////////////////

/**
 * Migrates execution to the nodes set in the nodemask.  Additionally, migrate
 * pages if NUMA_MIGRATE_EXISTING is set in flags.
 */
static int __numa_bind_and_migrate(struct bitmask* nm,
																	 numa_flag_t flags)
{
	int ret = 0;
	if(NUMA_DO_MIGRATE( flags ))
		ret = numa_migrate_pages(0, numa_get_membind(), nm);
	numa_bind(nm);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Initialize & teardown
///////////////////////////////////////////////////////////////////////////////

int numa_initialize(struct bitmask* mem_nodes,
										struct bitmask* exec_nodes,
										numa_flag_t flags)
{
	int result = 0;

	// Sanity checks:
	// Ensure NUMA availability on this system
	assert(numa_available() > -1 &&
		"NUMA support is not available on this system");
	assert((mem_nodes != NULL || exec_nodes != NULL) &&
					"Cannot initialize, either mem_nodes or exec_nodes are NULL!");
	assert((!numa_bitmask_equal(mem_nodes, numa_no_nodes_ptr) ||
					!numa_bitmask_equal(exec_nodes, numa_no_nodes_ptr)) &&
					"Cannot initialize, either mem_nodes or exec_nodes are empty!");

	//TODO save current configuration

	if(numa_bitmask_equal(mem_nodes, exec_nodes))
		result = __numa_bind_and_migrate(mem_nodes, flags);
	else
	{
		result = numa_run_on_node_mask(exec_nodes);
		numa_set_membind(mem_nodes);
	}
	
#ifdef _VERBOSE_NUMA
	//TODO
#endif

	return result;
}

int numa_initialize_node(numa_node_t mem_node,
												 numa_node_t exec_node,
												 numa_flag_t flags)
{
	int result = 0;

	// Sanity check
	assert(mem_node < numa_num_configured_nodes() &&
				 exec_node < numa_num_configured_nodes() &&
				 "Cannot intiialize, either mem_node or exec_node are invalid!");

	struct bitmask* mem_nodes = numa_allocate_nodemask();
	struct bitmask* exec_nodes = numa_allocate_nodemask();

	if(mem_node == exec_node)
	{
		numa_bitmask_setbit(mem_nodes, mem_node);
		numa_bitmask_setbit(exec_nodes, mem_node);
	}
	else
	{
		if(exec_node != ANY_NODE)
			numa_bitmask_setbit(exec_nodes, exec_node);
		else
			numa_bitmask_setall(exec_nodes);

		if(mem_node != ANY_NODE)
			numa_bitmask_setbit(mem_nodes, mem_node);
		else
			numa_bitmask_setall(mem_nodes);
	}

	result = numa_initialize(mem_nodes, exec_nodes, flags);
	numa_bitmask_free(mem_nodes);
	numa_bitmask_free(exec_nodes);
	return result;
}

int numa_initialize_env(numa_flag_t flags)
{
	// Assert that no more than one configuration was requested
	int result = 0, configs = 0;
	if(getenv(NUMA_BIND_TO_NODES)) configs++;
	if(getenv(NUMA_CPU_NODES) || getenv(NUMA_MEM_NODES)) configs++;
	assert(configs < 2 && "Please set either "
		NUMA_BIND_TO_NODES " or " NUMA_CPU_NODES " + " NUMA_MEM_NODES);

	struct bitmask* mem_nodes;
	struct bitmask* exec_nodes;
	int free_mem_nodes = 0;
	int free_exec_nodes = 0;

	if(getenv(NUMA_BIND_TO_NODES))
	{
		mem_nodes = numa_parse_nodestring(getenv(NUMA_BIND_TO_NODES));
		exec_nodes = numa_parse_nodestring(getenv(NUMA_BIND_TO_NODES));
		free_mem_nodes = 1;
		free_exec_nodes = 1;
	}
	else
	{
		if(getenv(NUMA_CPU_NODES))
		{
			exec_nodes = numa_parse_nodestring(getenv(NUMA_CPU_NODES));
			free_exec_nodes = 1;
		}
		else
			exec_nodes = numa_all_nodes_ptr;

		if(getenv(NUMA_MEM_NODES))
		{
			mem_nodes = numa_parse_nodestring(getenv(NUMA_MEM_NODES));
			free_mem_nodes = 1;
		}
		else
			mem_nodes = numa_all_nodes_ptr;
	}

	assert(mem_nodes != NULL && exec_nodes != NULL &&
		"Could not get nodes from environment - check for valid nodestrings!");

	result = numa_initialize(mem_nodes, exec_nodes, flags);
	if(free_mem_nodes)
		numa_bitmask_free(mem_nodes);
	if(free_exec_nodes)
		numa_bitmask_free(exec_nodes);
	return result;
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

	struct bitmask* nm = numa_allocate_nodemask();
	numa_bitmask_setbit(nm, node);
	int ret = __numa_bind_and_migrate(nm, flags);
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

void numa_mem_info(char* str, size_t str_size)
{
	char tmp[STR_BUF_SIZE];
	struct bitmask* nm = numa_get_membind();
	str[0] = '\0';

	numa_nodemask_to_str(nm, tmp, sizeof(tmp));
	snprintf(str, str_size, "Node(s): %s", tmp);

	numa_bitmask_free(nm);
}

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
	snprintf(tmp, sizeof(tmp), " (executing on CPU %d)", sched_getcpu());
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
	
	if(!first_set)
		snprintf(str, str_size, "(none)");
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

	if(!first_set)
		snprintf(str, str_size, "(none)");
}

