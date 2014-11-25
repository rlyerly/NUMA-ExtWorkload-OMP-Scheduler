#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

/* Shared-memory API */
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

/* Threading API */
#include <semaphore.h>

#include <sched_comm.h>

///////////////////////////////////////////////////////////////////////////////
// Internal definitions
///////////////////////////////////////////////////////////////////////////////

#define SHMEM_FILE "omp_numa"

/* Error-reporting */
#define INIT_PERROR( msg, handle ) { \
	perror(msg); \
	free(handle); \
	return NULL; \
}

#ifdef _VERBOSE
#define ERROR( msg ) fprintf(stderr, "ERROR: " msg)
#define WARN( msg ) fprintf(stderr, "WARNING: " msg)
#else
#define ERROR( msg ) {}
#define WARN( msg ) {}
#endif

/* Shared-memory data & application handle */
typedef struct omp_numa_shmem {
	/* Standard locking for concurrency updates */
	sem_t lock;

	/* Per-node task information:
	 *   1. Number of NUMA nodes
	 *   2. Vector of counters of OpenMP tasks for each node
	 */
	numa_node_t num_nodes;
	unsigned node_task_count[MAX_NUM_NODES];
} omp_numa_shmem;

struct omp_numa_t {
	int shmem_fd;
	struct stat shmem_fd_stats;

	/* Actual shared memory between processes */
	omp_numa_shmem* shmem;
};

///////////////////////////////////////////////////////////////////////////////
// Initialization & shutdown
///////////////////////////////////////////////////////////////////////////////

omp_numa_t* omp_numa_initialize()
{
	int must_initialize = 0;
	omp_numa_t* new_handle = (omp_numa_t*)malloc(sizeof(omp_numa_t));
	new_handle->shmem_fd = -1;
	new_handle->shmem = NULL;

	// Open shared-memory file
	new_handle->shmem_fd = shm_open(SHMEM_FILE, O_RDWR | O_CREAT, 0666);
	if(new_handle->shmem_fd < 0)
		INIT_PERROR("Could not open shared-memory device", new_handle);

	// Get size & initialize if necessary
	// NOTE: not thread-safe! Written with the expectation that a single process
	// is responsible for initializing & cleaning up the shared memory
	if(fstat(new_handle->shmem_fd, &new_handle->shmem_fd_stats))
		INIT_PERROR("Could not get shared-memory file statistics", new_handle);

	if(new_handle->shmem_fd_stats.st_size == 0)
	{
		// Resize
		must_initialize = 1;
		if(ftruncate(new_handle->shmem_fd, sizeof(omp_numa_shmem)))
			INIT_PERROR("Could not resize shared-memory file", new_handle);
	}

	// Map in memory
	if((new_handle->shmem = (omp_numa_shmem*)mmap(NULL,
																								sizeof(omp_numa_shmem),
																								PROT_WRITE,
																								MAP_SHARED,
																								new_handle->shmem_fd,
																								0))
																										== MAP_FAILED)
		INIT_PERROR("Could not map shared-memory into process", new_handle);

	if(must_initialize)
	{
		// Initialize semaphore & counters
		if(sem_init(&new_handle->shmem->lock, 1, 1))
			INIT_PERROR("Could not initialize semaphore", new_handle);
		omp_numa_clear_counters(new_handle);
	}

	return new_handle;
}

void omp_numa_shutdown(omp_numa_t* handle)
{
	munmap(handle->shmem, sizeof(omp_numa_shmem));
	close(handle->shmem_fd);
	shm_unlink(SHMEM_FILE);
	free(handle);
}

///////////////////////////////////////////////////////////////////////////////
// Current OpenMP/NUMA Information
///////////////////////////////////////////////////////////////////////////////

numa_node_t omp_numa_num_nodes(omp_numa_t* handle)
{
	return handle->shmem->num_nodes;
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

///////////////////////////////////////////////////////////////////////////////
// Updates
///////////////////////////////////////////////////////////////////////////////

void omp_numa_clear_counters(omp_numa_t* handle)
{
	sem_wait(&handle->shmem->lock);
	int i = 0;
	for(i = 0; i < MAX_NUM_NODES; i++)
		handle->shmem->node_task_count[i] = 0;
	sem_post(&handle->shmem->lock);
}

exec_spec_t* omp_numa_schedule_tasks(omp_numa_t* handle,
																		 exec_spec_t* requested,
																		 omp_numa_flags flags)
{
	assert(node < MAX_NUM_NODES);
	sem_wait(&handle->shmem->lock);
	exec_spec_t* result = NULL;

	if(requested != NULL)
	{
		numa_node_t cur_node = 0;
		for(cur_node = 0; cur_node < MAX_NUM_NODES; cur_node++)
			handle->shmem->node_task_count[cur_node] +=
				requested->task_assignment[cur_node];
		result = requested;
	}
	else
	{
		//TODO
		assert(false);
	}

	sem_post(&handle->shmem->lock);
	return result;
}

void omp_numa_cleanup(omp_numa_t* handle, exec_spec_t* spec)
{
	sem_wait(&handle->shmem->lock);
	numa_node_t cur_node = 0;
	for(cur_node = 0; cur_node < MAX_NUM_NODES; cur_node++)
		handle->shmem->node_task_count[cur_node] -=
			spec->task_assignment[cur_node];
	sem_post(&handle->shmem->lock);
}

///////////////////////////////////////////////////////////////////////////////
// Miscellaneous helpers
///////////////////////////////////////////////////////////////////////////////

/* N/A for now... */

