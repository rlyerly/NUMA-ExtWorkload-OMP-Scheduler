///////////////////////////////////////////////////////////////////////////////
// Shared-memory communication library for OpenMP applications               //
//                                                                           //
// Used to communicate which processes are executing, and on which NUMA      //
// nodes.                                                                    //
///////////////////////////////////////////////////////////////////////////////

#ifndef _SHARED_MEM_H
#define _SHARED_MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <numa_ctl.h>

///////////////////////////////////////////////////////////////////////////////
// Useful definitions
///////////////////////////////////////////////////////////////////////////////

#define MAX_NUM_NODES 64

/* Handle used to query and retain NUMA information about OpenMP tasks */
typedef struct omp_numa_t omp_numa_t;

/* Execution specification, returned by map_tasks() */
typedef struct exec_spec_t {
	unsigned num_tasks; // Total number of tasks
	unsigned task_assignment[MAX_NUM_NODES]; // Per-node tasks
} exec_spec_t;

/* Flag type for configuring behavior */
typedef unsigned omp_numa_flags;

/* OpenMP/NUMA debugging support */
//#define _OMP_NUMA_DEBUGGING 1
#undef _OMP_NUMA_DEBUGGING

#ifdef _OMP_NUMA_DEBUGGING
#define OMP_NUMA_DEBUG( format, ... ) \
	fprintf(stderr, "[%s, %d] OpenMP/NUMA: " format, __FILE__, __LINE__, \
		##__VA_ARGS__)
#else
#define OMP_NUMA_DEBUG( format, ... )
#endif

/* For initialize & shutdown, specify whether the calling process is
 * considered the shepherd process responsible for initial setup & final
 * cleanup.
 */
#define IS_SHEPHERD( flags ) (flags & 0x1)
#define SHEPHERD 1
#define NOT_SHEPHERD 0

/* For query operations, specify whether to lock or not */
#define DO_FAST_CHECK( flags ) ((flags >> 1) & 0x1)
#define FAST_CHECK 1 << 1
#define NO_FAST_CHECK 0 << 1

/* Environment variables to specify configuration & execution */
#define OMP_NUMA_AWARE_MAPPING "OMP_NUMA_AWARE_MAPPING"

///////////////////////////////////////////////////////////////////////////////
// Initialization & shutdown
///////////////////////////////////////////////////////////////////////////////

/**
 * Initialize shared memory communication
 *
 * @return a shared-memory handle used for communication, or NULL if
 *         initialization failed
 */
omp_numa_t* omp_numa_initialize(omp_numa_flags flags);

/**
 * Shutdown shared-memory communication
 *
 * @param handle the shared-memory handle to shutdown & clean up
 */
void omp_numa_shutdown(omp_numa_t* handle, omp_numa_flags);

///////////////////////////////////////////////////////////////////////////////
// Current OpenMP/NUMA information
///////////////////////////////////////////////////////////////////////////////

/**
 * Returns the number of nodes
 */
numa_node_t omp_numa_num_nodes();

/**
 * Returns the number of processors
 */
unsigned omp_numa_num_procs();

/**
 * Returns the number of processors per node
 */
unsigned omp_numa_num_procs_per_node();

/**
 * Return the number of tasks currently executing on a NUMA node
 *
 * @param handle the shared-memory handle
 * @param node the node for which to query the number of tasks
 * @param flags users can specify OMP_FAST to avoid locking
 * @return the number of OpenMP tasks executing on a node
 */
int omp_numa_num_tasks(omp_numa_t* handle, numa_node_t node, omp_numa_flags flags);

/**
 * Populate an array with the current task assignment for all NUMA nodes.
 *
 * @param handle the shared-memory handle
 * @param task_assignment an array to be populated with current task assignment
 * @param num_nodes number of elements in task_assignment
 * @param flags users can specify OMP_FAST to avoid locking
 */
void omp_numa_task_assignment(omp_numa_t* handle,
															unsigned* task_assignment,
															size_t num_nodes,
															omp_numa_flags flags);

///////////////////////////////////////////////////////////////////////////////
// Updates to shared data
///////////////////////////////////////////////////////////////////////////////

/**
 * Clear all node counters counters
 *
 * @param handle the shared-memory handle
 */
void omp_numa_clear_counters(omp_numa_t* handle);

/**
 * Schedule tasks for an OpenMP application
 *
 * TODO describe strategy for mapping
 *
 * @param handle the shared-memory handle
 * @param requested_spec application-requested execution specification.  If
 *        non-null, then the library updates the shared memory with the
 *        requested specification (and ignores some flags).
 * @param flags configure mapping behavior (ignored for now)
 */
exec_spec_t* omp_numa_map_tasks(omp_numa_t* handle,
																exec_spec_t* requested,
																omp_numa_flags flags);

/**
 * Cleanup an application's task from the node task counters
 *
 * @param handle the shared-memory handle
 * @param node the node for which to update task counters
 * @param num_tasks the number of tasks this application had executing on a
 *        node
 */
void omp_numa_cleanup(omp_numa_t* handle, exec_spec_t* spec);

///////////////////////////////////////////////////////////////////////////////
// Miscellaneous helpers
///////////////////////////////////////////////////////////////////////////////

/* N/A for now... */

#ifdef __cplusplus
}
#endif

#endif /* _SHARED_MEM_H */

