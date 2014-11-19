/**
 * NUMA support - allows processes to query system for NUMA support, topology,
 * current memory policy, and other NUMA-related things.
 *
 * Author: Rob Lyerly
 * Date: Wed November 12 2014
 */

/**
 * Compilation notes: link against -lnuma
 */

#ifndef _NUMA_CTL_H
#define _NUMA_CTL_H

#include <stdint.h>
#include <numa.h>

/* Flags for configuring behavior */
typedef uint32_t numa_flag_t;
#define NUMA_MIGRATE 1
#define NUMA_NO_MIGRATE 0

/* Environment variables specifying NUMA behavior */
#define ENV_BIND_TO_NODE "BIND_TO_NODE" // Bind CPU & memory to node
#define ENV_CPU_NODE "CPU_NODE" // Execute on node
#define ENV_MEM_NODE "MEM_NODE" // Memory on node

/* Library configuration */
#define STR_BUF_SIZE 256

///////////////////////////////////////////////////////////////////////////////
// Initialize & shutdown
///////////////////////////////////////////////////////////////////////////////

int numa_initialize();
void numa_shutdown();

///////////////////////////////////////////////////////////////////////////////
// Process & memory migration
///////////////////////////////////////////////////////////////////////////////

/**
 * Notes (see "man 3 numa" for more information):
 * - To migrate execution, use "int numa_run_on_node(int node)"
 */

/**
 * Bind memory & execution of a task to a NUMA node.  Optionally, migrate
 * task's pages.
 * 
 * @param node NUMA node in which to bind this task's memory & execution
 * @param migrate flag indicating whether or not to migrate current memory
 * @return true if bind succeeded, false otherwise
 */
int numa_bind_node(unsigned int node, numa_flag_t migrate);

/**
 * Bind memory of a task to a NUMA node.  Optionally, migrate task's pages.
 *
 * @param node NUMA node in which to bind this task's memory
 * @param migrate flag indicating whether ot not to migrate current memory
 * @param true if bind succeeded, false otherwise
 */
int numa_set_membind_node(unsigned int node, numa_flag_t migrate);

///////////////////////////////////////////////////////////////////////////////
// Convenience functions
///////////////////////////////////////////////////////////////////////////////

void numa_task_info();
void numa_nodemask_to_str(struct bitmask* nodes, char* str, size_t str_size);
void numa_cpumask_to_str(struct bitmask* cpus, char* str, size_t str_size);

#endif /* _NUMA_CTL_H */

