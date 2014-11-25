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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>
#include <numa.h>

///////////////////////////////////////////////////////////////////////////////
// Typedefs, definitions & macros
///////////////////////////////////////////////////////////////////////////////

/* NUMA typedefs */
typedef int numa_node_t;
typedef unsigned int numa_flag_t;

/* Useful definitions */
#define NO_FLAGS 0
#define ANY_NODE INT_MAX
#define IS_BIT_SET( flags, pos ) ((flags >> pos ) & 0x1)

/* Migration */
#define NUMA_DO_MIGRATE( flags ) IS_BIT_SET(flags, 0)
#define NUMA_MIGRATE_EXISTING 1

/* Configuration from environment variables */
#define NUMA_ENV_CONFIG( flags ) IS_BIT_SET(flags, 1)
#define NUMA_ENV 1 << 1
#define NUMA_NO_ENV 0 << 1
#define NUMA_BIND_TO_NODES "NUMA_BIND_TO_NODES" // Bind CPU & memory to node
#define NUMA_CPU_NODES "NUMA_CPU_NODES" // Execute on node
#define NUMA_MEM_NODES "NUMA_MEM_NODES" // Memory on node

/* Other library configuration */
#define STR_BUF_SIZE 256

///////////////////////////////////////////////////////////////////////////////
// Initialize & shutdown
///////////////////////////////////////////////////////////////////////////////
 
/**
 * Initializes NUMA control - checks for availability of NUMA & migrates tasks
 * & memory to requested nodes.
 *
 * If NUMA_MIGRATE_EXISTING is set in flags, then a task's existing pages are
 * migrated to the new nodeset.
 *
 * @param mem_node NUMA nodeset used for memory allocation
 * @param exec_node NUMA nodeset used for execution, execute on CPUs for that
 *                  node
 * @param flags configure behavior through flags:
 *          NUMA_MIGRATE_EXISTING: migrate task's existing pages to new nodeset
 * @return true if initialization was successful, false otherwise
 */
int numa_initialize(struct bitmask* mem_nodes,
										struct bitmask* exec_nodes,
										numa_flag_t flags);

/**
 * A shorthand for numa_initiliaze() - allows users to specify a single node
 * number rather than constructing nodesets.
 *
 * @param mem_node NUMA node used for memory allocation
 * @param exec_node NUMA node used for execution
 * @param flags configure behavior through flags:
 *          NUMA_MIGRATE_EXISTING: migrate task's existing pages to new node
 * @return true if initialization was successful, false otherwise
 */
int numa_initialize_node(numa_node_t mem_node,
												 numa_node_t exec_node,
												 numa_flag_t flags);

/**
 * A shorthand for numa_initialize() - configure NUMA behavior from environment
 * variables.  Users should set either NUMA_BIND_TO_NODES or a combination of
 * NUMA_CPU_NODE & NUMA_MEM_NODE to a string parsable by
 * numa_parse_nodestring() to configure behavior.
 *
 * If NUMA_BIND_TO_NODE is set, bind both memory & execution to the specified
 * nodeset.
 *
 * Otherwise, bind execution to the nodeset specified by NUMA_CPU_NODES & bind
 * memory to the nodeset specified by NUMA_MEM_NODES.  If they are equal, it is
 * is equivalent to setting NUMA_BIND_TO_NODE.  Setting one and not the other
 * initializes the unspecified nodeset to numa_all_nodes_ptr.
 *
 * @param flags configure behavior through flags:
 *          NUMA_MIGRATE_EXISTING: migrate task's existing pages to new node
 * @return true if initialization was successful, false otherwise
 */
int numa_initialize_env(numa_flag_t flags);

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
 * @return 0 if bind succeeded, false otherwise
 */
int numa_bind_node(numa_node_t node, numa_flag_t flags);

/**
 * Bind memory of a task to a NUMA node.  Optionally, migrate task's pages.
 *
 * @param node NUMA node in which to bind this task's memory
 * @param flags flag indicating whether to not to migrate current memory
 * @param 0 if bind succeeded, false otherwise
 */
int numa_set_membind_node(numa_node_t node, numa_flag_t flags);

///////////////////////////////////////////////////////////////////////////////
// Convenience functions
///////////////////////////////////////////////////////////////////////////////

/**
 * Write current NUMA memory information to a string.
 */
void numa_mem_info(char* str, size_t str_size);

/**
 * Write current NUMA task execution information to a string.
 */
void numa_task_info(char* str, size_t str_size);

/**
 * Convert a node mask into a CPU mask with all CPUs contained by the nodes in
 * the node mask.
 */
void numa_nodemask_to_cpumask(const struct bitmask* nodes, struct bitmask* cpus);

/**
 * Convert a node mask to a string.
 */
void numa_nodemask_to_str(const struct bitmask* nodes, char* str, size_t str_size);

/**
 * Convert a CPU mask to a string.
 */
void numa_cpumask_to_str(const struct bitmask* cpus, char* str, size_t str_size);

#ifdef __cplusplus
}
#endif

#endif /* _NUMA_CTL_H */

