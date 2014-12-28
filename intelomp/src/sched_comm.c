#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

/* Shared-memory API */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Threading API */
#include <semaphore.h>
#include <pthread.h>
#include <sys/sysinfo.h>

#include <sched_comm.h>

///////////////////////////////////////////////////////////////////////////////
// Internal definitions
///////////////////////////////////////////////////////////////////////////////

/* Specify lock type (only leave one uncommented!) */
#define _USE_SPINLOCK
//#define _USE_SEMAPHORE

#ifdef _USE_SPINLOCK
#ifdef _USE_SEMAPHORE
#error Please use either a spinlock or a semaphore!
#endif
#endif

#define SHMEM_FILE "omp_numa"
#define MAX( a, b ) (a > b ? a : b)
#define MIN( a, b ) (a < b ? a : b)

/* Error-reporting */
#define INIT_PERROR( msg, handle ) { \
	perror(msg); \
	free(handle); \
	return NULL; \
}

#define ERROR( msg ) fprintf(stderr, "ERROR: " msg)

#ifdef _VERBOSE
#define WARN( msg ) fprintf(stderr, "WARNING: " msg)
#else
#define WARN( msg )
#endif

static numa_node_t __num_nodes;
static unsigned __num_procs;
static unsigned __num_procs_per_node;

/* Shared-memory data & application handle */
typedef struct omp_numa_shmem {
	/* POSIX locking for concurrency updates */
#ifdef _USE_SPINLOCK
	pthread_spinlock_t lock;
#else
	sem_t lock;
#endif

	/* Runtime environment information:
	 *   1. Number of OpenMP applications
	 *   2. Number of tasks for all OpenMP applications
	 *   3. Current NUMA node, for filling in a round-robin fasion
	 */
	unsigned num_omp_applications;
	unsigned num_omp_tasks;
	numa_node_t cur_rr_node; // TODO needed?

	/* Per-node task information:
	 *   1. Per-node OpenMP application count (# applications mapped to node)
	 *   2. Per-node OpenMP task counters (# tasks mapped to node)
	 */
	unsigned node_application_count[MAX_NUM_NODES];
	unsigned node_task_count[MAX_NUM_NODES];
} omp_numa_shmem;

struct omp_numa_t {
	int shmem_fd;
	struct stat shmem_fd_stats;

	/* Previous execution setup - used to attempt to place nodes near memory 
	 * from previous executions
	 */
	exec_spec_t prev_setup;

	/* Actual shared memory between processes */
	omp_numa_shmem* shmem;
};

///////////////////////////////////////////////////////////////////////////////
// Prototypes for internal functions
///////////////////////////////////////////////////////////////////////////////

static unsigned calc_num_tasks(omp_numa_t* handle, omp_numa_flags flags);
static exec_spec_t* map_tasks_to_nodes(omp_numa_t* handle,
																			 unsigned num_tasks,
																			 omp_numa_flags flags);

///////////////////////////////////////////////////////////////////////////////
// Initialization & shutdown
///////////////////////////////////////////////////////////////////////////////

omp_numa_t* omp_numa_initialize(omp_numa_flags flags)
{
	int i = 0;
	omp_numa_t* new_handle = (omp_numa_t*)malloc(sizeof(omp_numa_t));
	new_handle->shmem_fd = -1;
	new_handle->shmem = NULL;

	// Open shared-memory file
	if(IS_SHEPHERD(flags))
	{
		OMP_NUMA_DEBUG("initializing OpenMP/NUMA handle (shepherd)\n");
		new_handle->shmem_fd = shm_open(SHMEM_FILE, O_RDWR | O_CREAT | O_EXCL, 0666);
	}
	else
	{
		OMP_NUMA_DEBUG("initializing OpenMP/NUMA handle (non-shepherd)\n");
		new_handle->shmem_fd = shm_open(SHMEM_FILE, O_RDWR, 0666);
	}

	if(new_handle->shmem_fd < 0)
		INIT_PERROR("Could not open shared-memory device (is the shepherd running?)",
			new_handle);

	// Get size & initialize if necessary
	// NOTE: not thread-safe! Written with the expectation that a single process
	// is responsible for initializing & cleaning up the shared memory
	if(fstat(new_handle->shmem_fd, &new_handle->shmem_fd_stats))
		INIT_PERROR("Could not get shared-memory file statistics", new_handle);

	// Resize
	if(IS_SHEPHERD(flags))
		if(ftruncate(new_handle->shmem_fd, sizeof(omp_numa_shmem)))
			INIT_PERROR("Could not resize shared-memory file", new_handle);

	// Map into memory
	if((new_handle->shmem = (omp_numa_shmem*)mmap(NULL,
																								sizeof(omp_numa_shmem),
																								PROT_WRITE,
																								MAP_SHARED,
																								new_handle->shmem_fd,
																								0))
																										== MAP_FAILED)
		INIT_PERROR("Could not map shared-memory into process", new_handle);

	if(IS_SHEPHERD(flags))
	{
		// Initialize lock
#ifdef _USE_SPINLOCK
		if(pthread_spin_init(&new_handle->shmem->lock, PTHREAD_PROCESS_SHARED))
			INIT_PERROR("Could not initialize spin lock", new_handle);
		pthread_spin_lock(&new_handle->shmem->lock);
#else
		if(sem_init(&new_handle->shmem->lock, 1, 1))
			INIT_PERROR("Could not initialize semaphore", new_handle);
		sem_wait(&new_handle->shmem->lock);
#endif

		// Initialize shared memory
		new_handle->shmem->num_omp_applications = 0;
		new_handle->shmem->num_omp_tasks = 0;
		new_handle->shmem->cur_rr_node = 0;

		for(i = 0; i < __num_nodes; i++)
		{
			new_handle->shmem->node_application_count[i] = 0;
			new_handle->shmem->node_task_count[i] = 0;
		}

#ifdef _USE_SPINLOCK
		pthread_spin_unlock(&new_handle->shmem->lock);
#else
		sem_post(&new_handle->shmem->lock);
#endif
	}

	new_handle->prev_setup.num_tasks = 0;
	for(i = 0; i < __num_nodes; i++)
		new_handle->prev_setup.task_assignment[i] = 1;

	// Initialize internal values
	__num_nodes = MIN(numa_num_configured_nodes(), MAX_NUM_NODES);
	__num_procs = get_nprocs();
	__num_procs_per_node = __num_procs / __num_nodes; //Assuming even number...

	return new_handle;
}

void omp_numa_shutdown(omp_numa_t* handle, omp_numa_flags flags)
{
	OMP_NUMA_DEBUG("shutting down\n");
	munmap(handle->shmem, sizeof(omp_numa_shmem));
	close(handle->shmem_fd);
	if(IS_SHEPHERD(flags))
	{
#ifdef _USE_SPINLOCK
		pthread_spin_destroy(&handle->shmem->lock);
#else
		sem_destroy(&handle->shmem->lock);
#endif
		shm_unlink(SHMEM_FILE);
	}
	free(handle);
}

///////////////////////////////////////////////////////////////////////////////
// Current OpenMP/NUMA Information
///////////////////////////////////////////////////////////////////////////////

numa_node_t omp_numa_num_nodes()
{
	return __num_nodes;
}

unsigned omp_numa_num_procs()
{
	return __num_procs;
}

unsigned omp_numa_num_procs_per_node()
{
	return __num_procs_per_node;
}

int omp_numa_num_tasks(omp_numa_t* handle, numa_node_t node, omp_numa_flags flags)
{
	assert(node < MAX_NUM_NODES);

	if(DO_FAST_CHECK(flags)) // Get potentially outdated value
		return handle->shmem->node_task_count[node];
	else // Get guaranteed up-to-date value
	{
		int result = -1;
#ifdef _USE_SPINLOCK
		pthread_spin_lock(&handle->shmem->lock);
#else
		sem_wait(&handle->shmem->lock);
#endif
		result = handle->shmem->node_task_count[node];
#ifdef _USE_SPINLOCK
		pthread_spin_unlock(&handle->shmem->lock);
#else
		sem_post(&handle->shmem->lock);
#endif
		return result;
	}
}

void omp_numa_task_assignment(omp_numa_t* handle,
															unsigned* task_assignment,
															size_t num_nodes,
															omp_numa_flags flags)
{
	unsigned i, num_elems = MIN(num_nodes, (unsigned)__num_nodes);

	if(DO_FAST_CHECK(flags))
	{
		for(i = 0; i < num_elems; i++)
			task_assignment[i] = handle->shmem->node_task_count[i];
	}
	else
	{
#ifdef _USE_SPINLOCK
		pthread_spin_lock(&handle->shmem->lock);
#else
		sem_wait(&handle->shmem->lock);
#endif
		for(i = 0; i < num_elems; i++)
			task_assignment[i] = handle->shmem->node_task_count[i];
#ifdef _USE_SPINLOCK
		pthread_spin_unlock(&handle->shmem->lock);
#else
		sem_post(&handle->shmem->lock);
#endif
	}
}

///////////////////////////////////////////////////////////////////////////////
// Updates
///////////////////////////////////////////////////////////////////////////////

void omp_numa_clear_counters(omp_numa_t* handle)
{
#ifdef _USE_SPINLOCK
	pthread_spin_lock(&handle->shmem->lock);
#else
	sem_wait(&handle->shmem->lock);
#endif
	OMP_NUMA_DEBUG("clearing all node counters\n");

	int i = 0;
	for(i = 0; i < __num_nodes; i++)
		handle->shmem->node_task_count[i] = 0;
#ifdef _USE_SPINLOCK
	pthread_spin_unlock(&handle->shmem->lock);
#else
	sem_post(&handle->shmem->lock);
#endif
}

exec_spec_t* omp_numa_map_tasks(omp_numa_t* handle,
																exec_spec_t* requested,
																omp_numa_flags flags)
{
#ifdef _USE_SPINLOCK
	pthread_spin_lock(&handle->shmem->lock);
#else
	sem_wait(&handle->shmem->lock);
#endif

	exec_spec_t* result;
	handle->shmem->num_omp_applications++; // Needed before calc_num_tasks
	if(!requested)
	{
		result = map_tasks_to_nodes(handle,
																calc_num_tasks(handle, flags),
																flags);
		OMP_NUMA_DEBUG("mapping %d threads\n", result->num_tasks);
	}
	else
	{
		result = requested;
		OMP_NUMA_DEBUG("mapping %d threads (user-requested)\n", result->num_tasks);
	}

	handle->shmem->num_omp_tasks += result->num_tasks;
	numa_node_t cur_node = 0;
	for(cur_node = 0; cur_node < __num_nodes; cur_node++)
	{
		if(result->task_assignment[cur_node])
		{
			handle->shmem->node_application_count[cur_node]++;
			handle->shmem->node_task_count[cur_node] +=
				result->task_assignment[cur_node];
		}
	}

#ifdef _USE_SPINLOCK
	pthread_spin_unlock(&handle->shmem->lock);
#else
	sem_post(&handle->shmem->lock);
#endif
	return result;
}

void omp_numa_cleanup(omp_numa_t* handle, exec_spec_t* spec)
{
#ifdef _USE_SPINLOCK
	pthread_spin_lock(&handle->shmem->lock);
#else
	sem_wait(&handle->shmem->lock);
#endif
	OMP_NUMA_DEBUG("cleaning up (%d tasks)\n", spec->num_tasks);
	handle->shmem->num_omp_applications--;
	handle->shmem->num_omp_tasks -= spec->num_tasks;
	numa_node_t cur_node = 0;
	for(cur_node = 0; cur_node < __num_nodes; cur_node++)
	{
		if(spec->task_assignment[cur_node])
		{
			handle->shmem->node_application_count[cur_node]--;
			handle->shmem->node_task_count[cur_node] -=
				spec->task_assignment[cur_node];
		}
	}
#ifdef _USE_SPINLOCK
	pthread_spin_unlock(&handle->shmem->lock);
#else
	sem_post(&handle->shmem->lock);
#endif

	// Save previous setup for NUMA-aware mapping
	handle->prev_setup.num_tasks = spec->num_tasks;
	int i;
	for(i = 0; i < __num_nodes; i++)
		handle->prev_setup.task_assignment[i] = spec->task_assignment[i];
}

///////////////////////////////////////////////////////////////////////////////
// Miscellaneous helpers
///////////////////////////////////////////////////////////////////////////////

// TODO make configurable, add other options (i.e. ML)
/* Give each application an equal number of processors, that is:
 *
 *   # processors / # OpenMP applications
 */
unsigned calc_num_tasks(omp_numa_t* handle, omp_numa_flags flags)
{
	if(handle->shmem->num_omp_applications == 0)
		return __num_procs;
	else
		return ceil((double)__num_procs /
								(double)handle->shmem->num_omp_applications);
}

// TODO make configurable, add other options
/* Assign the requested number of tasks to nodes in a round-robin fasion -
 * fill each node up with tasks, then move on to the next.
 *
 * "Filling up a node" refers to mapping up to __num_procs_per_node tasks
 * to a NUMA node.  If it has fewer tasks than this, it is considered
 * unfilled.
 *
 * The current algorithm maps tasks according to the following priority:
 *
 * 1. (NUMA-aware) Map tasks to nodes that are empty and on which we've
 *    previously executed
 * 2. (NUMA-aware) Map tasks to nodes that are unfilled and on which we've
 *    previously executed
 * 3. Map tasks to nodes that are empty
 * 4. Map tasks to nodes that are unfilled
 * 5. Map tasks to nodes that have the least number of nodes.
 *    (NUMA-aware) if two tasks have equally small numbers of tasks, prefer the
 *    node on which we've previously executed
 *
 * TODO several of these priorities can be collapsed into single priorities,
 * i.e. 1 & 2 could be done in the same for-loop.  This isn't a performance
 * concern right now, as we've got maximum 8 nodes in our test setup.
 */
exec_spec_t* map_tasks_to_nodes(omp_numa_t* handle,
																unsigned num_tasks,
																omp_numa_flags flags)
{
	exec_spec_t* spec = (exec_spec_t*)malloc(sizeof(exec_spec_t));
	unsigned tasks_remaining = num_tasks;
	unsigned task_chunk;
	numa_node_t cur_node;
	unsigned local_task_count[MAX_NUM_NODES];

	// Check to see if NUMA-aware mapping is enabled
	int numa_aware = 0;
	if(getenv(OMP_NUMA_AWARE_MAPPING) &&
		 !strcmp(getenv(OMP_NUMA_AWARE_MAPPING), "1"))
	{
		OMP_NUMA_DEBUG("NUMA-aware mapping is enabled\n");
		numa_aware = 1;
	}

	// Initialize local copy of current task assignment & spec task assignment
	spec->num_tasks = num_tasks;
	for(cur_node = 0; cur_node < __num_nodes; cur_node++)
	{
		spec->task_assignment[cur_node] = 0;
		local_task_count[cur_node] = handle->shmem->node_task_count[cur_node];
	}

	// NUMA-aware passes - attempt to schedule onto nodes that are not full and
	// on which we've previously executed
	if(numa_aware)
	{
		// First pass - attempt to map tasks into empty & previous execution nodes
		for(cur_node = 0; cur_node < __num_nodes && tasks_remaining; cur_node++)
		{
			if(local_task_count[cur_node] == 0 &&
				 handle->prev_setup.task_assignment[cur_node] != 0)
			{
				task_chunk = MIN(tasks_remaining, __num_procs_per_node);
				spec->task_assignment[cur_node] += task_chunk;
				local_task_count[cur_node] += task_chunk;
				tasks_remaining -= task_chunk;
			}
		}

		// Second pass - attempt to map tasks into non-full & previous execution
		// nodes
		for(cur_node = 0; cur_node < __num_nodes && tasks_remaining; cur_node++)
		{
			if(local_task_count[cur_node] < __num_procs_per_node &&
				 handle->prev_setup.task_assignment[cur_node] != 0)
			{
				task_chunk = MIN(tasks_remaining,
												 __num_procs_per_node - local_task_count[cur_node]);
				spec->task_assignment[cur_node] += task_chunk;
				local_task_count[cur_node] += task_chunk;
				tasks_remaining -= task_chunk;
			}
		}
	}

	// First pass - attempt to map tasks into empty nodes
	for(cur_node = 0; cur_node < __num_nodes && tasks_remaining; cur_node++)
	{
		if(local_task_count[cur_node] == 0)
		{
			task_chunk = MIN(tasks_remaining, __num_procs_per_node);
			spec->task_assignment[cur_node] += task_chunk;
			local_task_count[cur_node] += task_chunk;
			tasks_remaining -= task_chunk;
		}
	}

	// Second pass - attempt to map tasks into nodes that aren't already
	// fully occupied
	for(cur_node = 0; cur_node < __num_nodes && tasks_remaining; cur_node++)
	{
		if(local_task_count[cur_node] < __num_procs_per_node)
		{
			task_chunk =
				MIN(tasks_remaining, __num_procs_per_node - local_task_count[cur_node]);
			spec->task_assignment[cur_node] += task_chunk;
			local_task_count[cur_node] += task_chunk;
			tasks_remaining -= task_chunk;
		}
	}

	// Last pass - map tasks onto nodes to minimize oversubscription
	while(tasks_remaining)
	{
		numa_node_t cur_smallest = 0;
		unsigned cur_smallest_count = UINT_MAX;

		// Find node with smallest # tasks, or if we're NUMA-aware, a node with
		// an equally small # tasks but was a previous execution node
		for(cur_node = 0; cur_node < __num_nodes; cur_node++)
		{
			if(local_task_count[cur_node] < cur_smallest_count)
			{
				cur_smallest = cur_node;
				cur_smallest_count = local_task_count[cur_node];
			}
			else if(numa_aware &&
							local_task_count[cur_node] == cur_smallest_count &&
							handle->prev_setup.task_assignment[cur_node] != 0)
				cur_smallest = cur_node;
		}

		// Schedule tasks to fill up node to next multiple of __num_procs_per_node
		task_chunk = MIN(tasks_remaining,
			__num_procs_per_node -
				(local_task_count[cur_smallest] % __num_procs_per_node));
		spec->task_assignment[cur_smallest] += task_chunk;
		local_task_count[cur_smallest] += task_chunk;
		tasks_remaining -= task_chunk;
	}

	return spec;
}

