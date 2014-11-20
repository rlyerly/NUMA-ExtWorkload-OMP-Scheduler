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
#include <limits.h>
#include <numa.h>

///////////////////////////////////////////////////////////////////////////////
// Typedefs, definitions & macros
///////////////////////////////////////////////////////////////////////////////

/* NUMA typedefs */
typedef unsigned int numa_node_t;
typedef unsigned int numa_flag_t;

/* Useful definitions */
#define CURRENT_NODE UINT_MAX

/* Migration */
#define NUMA_DO_MIGRATE( val ) (val & 0x1)
#define NUMA_MIGRATE 1
#define NUMA_NO_MIGRATE 0

/* Enable/disable environment variables */
#define NUMA_ENV_CONFIG( val ) ((val >> 1) & 0x1)
#define NUMA_ENV 1 << 1
#define NUMA_NO_ENV 0 << 1

/* Environment variables specifying NUMA memory & execution behavior */
#define NUMA_BIND_TO_NODE "NUMA_BIND_TO_NODE" // Bind CPU & memory to node
#define NUMA_CPU_NODE "NUMA_CPU_NODE" // Execute on node
#define NUMA_MEM_NODE "NUMA_MEM_NODE" // Memory on node

/* Other library configuration */
#define STR_BUF_SIZE 256

///////////////////////////////////////////////////////////////////////////////
// Initialize & shutdown
///////////////////////////////////////////////////////////////////////////////

/**
 * Initializes NUMA control - checks for availability of NUMA & migrates tasks
 * & memory to requested nodes either through arguments or environment
 * variables specified above.
 *
 * If both mem_node & exec_node are set to CURRENT_NODE, then environment
 * variables are checked for configuration.  If no environment variables are
 * set, then current behavior is used.
 *
 * @param mem_node NUMA node used for memory allocation
 * @param exec_node NUMA node used for execution, execute on CPUs for that node
 * @return true
 */
int numa_initialize(numa_node_t mem_node,
										numa_node_t exec_node,
										numa_flag_t flags);

/**
 * Shut down NUMA control (does nothing for now).
 */
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
 * @param flags flag indicating whether or not to migrate current memory
 * @return true if bind succeeded, false otherwise
 */
int numa_bind_node(numa_node_t node, numa_flag_t flags);

/**
 * Bind memory of a task to a NUMA node.  Optionally, migrate task's pages.
 *
 * @param node NUMA node in which to bind this task's memory
 * @param flags flag indicating whether to not to migrate current memory
 * @param true if bind succeeded, false otherwise
 */
int numa_set_membind_node(numa_node_t node, numa_flag_t flags);

///////////////////////////////////////////////////////////////////////////////
// Convenience functions
///////////////////////////////////////////////////////////////////////////////

void numa_task_info(char* str, size_t str_size);
void numa_nodemask_to_cpumask(const struct bitmask* nodes, struct bitmask* cpus);
void numa_nodemask_to_str(const struct bitmask* nodes, char* str, size_t str_size);
void numa_cpumask_to_str(const struct bitmask* cpus, char* str, size_t str_size);

#endif /* _NUMA_CTL_H */

