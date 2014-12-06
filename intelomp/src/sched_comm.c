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
#include <sys/sysinfo.h>

#include <sched_comm.h>

///////////////////////////////////////////////////////////////////////////////
// Internal definitions
///////////////////////////////////////////////////////////////////////////////

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
	sem_t lock;

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
		// Initialize semaphore
		if(sem_init(&new_handle->shmem->lock, 1, 1))
			INIT_PERROR("Could not initialize semaphore", new_handle);
		sem_wait(&new_handle->shmem->lock);

		// Initialize shared memory
		new_handle->shmem->num_omp_applications = 0;
		new_handle->shmem->num_omp_tasks = 0;
		new_handle->shmem->cur_rr_node = 0;

		for(i = 0; i < __num_nodes; i++)
		{
			new_handle->shmem->node_application_count[i] = 0;
			new_handle->shmem->node_task_count[i] = 0;
		}

		sem_post(&new_handle->shmem->lock);
	}

	new_handle->prev_setup.num_tasks = 0;
	for(i = 0; i < __num_nodes; i++)
		new_handle->prev_setup.task_assignment[i] = 0;

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
		shm_unlink(SHMEM_FILE);
	free(handle);
}

///////////////////////////////////////////////////////////////////////////////
// Current OpenMP/NUMA Information
///////////////////////////////////////////////////////////////////////////////

numa_node_t omp_numa_num_nodes(omp_numa_t* handle)
{
	return __num_nodes;
}

int omp_numa_num_tasks(omp_numa_t* handle, numa_node_t node, omp_numa_flags flags)
{
	assert(node < MAX_NUM_NODES);

	if(DO_FAST_CHECK(flags)) // Get potentially outdated value
		return handle->shmem->node_task_count[node];
	else // Get guaranteed up-to-date value
	{
		int result = -1;
		sem_wait(&handle->shmem->lock);
		result = handle->shmem->node_task_count[node];
		sem_post(&handle->shmem->lock);
		return result;
	}
}

void omp_numa_task_assignment(omp_numa_t* handle,
															unsigned* task_assignment,
															size_t num_nodes,
															omp_numa_flags flags)
{
	int i, num_elems = MIN(num_nodes, __num_nodes);

	if(DO_FAST_CHECK(flags))
	{
		for(i = 0; i < num_elems; i++)
			task_assignment[i] = handle->shmem->node_task_count[i];
	}
	else
	{
		sem_wait(&handle->shmem->lock);
		for(i = 0; i < num_elems; i++)
			task_assignment[i] = handle->shmem->node_task_count[i];
		sem_post(&handle->shmem->lock);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Updates
///////////////////////////////////////////////////////////////////////////////

void omp_numa_clear_counters(omp_numa_t* handle)
{
	sem_wait(&handle->shmem->lock);
	OMP_NUMA_DEBUG("clearing all node counters\n");

	int i = 0;
	for(i = 0; i < __num_nodes; i++)
		handle->shmem->node_task_count[i] = 0;
	sem_post(&handle->shmem->lock);
}

exec_spec_t* omp_numa_map_tasks(omp_numa_t* handle,
																exec_spec_t* requested,
																omp_numa_flags flags)
{
	sem_wait(&handle->shmem->lock);

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

	sem_post(&handle->shmem->lock);
	return result;
}

void omp_numa_cleanup(omp_numa_t* handle, exec_spec_t* spec)
{
	sem_wait(&handle->shmem->lock);
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
	sem_post(&handle->shmem->lock);

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

	// Initialize local copy of current task assignment & spec task assignment
	spec->num_tasks = num_tasks;
	for(cur_node = 0; cur_node < __num_nodes; cur_node++)
	{
		spec->task_assignment[cur_node] = 0;
		local_task_count[cur_node] = handle->shmem->node_task_count[cur_node];
	}

	// TODO be "NUMA-aware" - attempt to map tasks close to previously
	// executed nodes
	if(getenv(OMP_NUMA_AWARE_MAPPING))
	{
		const char* val = getenv(OMP_NUMA_AWARE_MAPPING);

		if(!strcmp(val, "1"))
			assert(false && "OMP_NUMA_AWARE_MAPPING not yet implemented!");
	}

	// First pass - attempt to map tasks into empty nodes
	for(cur_node = 0; cur_node < __num_nodes && tasks_remaining; cur_node++)
	{
		task_chunk = MIN(tasks_remaining, __num_procs_per_node);
		if(local_task_count[cur_node] == 0)
		{
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
		// Find node with smallest # tasks
		numa_node_t cur_smallest = 0;
		unsigned cur_smallest_count = UINT_MAX;
		for(cur_node = 0; cur_node < __num_nodes; cur_node++)
		{
			if(local_task_count[cur_node] < cur_smallest_count)
			{
				cur_smallest = cur_node;
				cur_smallest_count = local_task_count[cur_node];
			}
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

