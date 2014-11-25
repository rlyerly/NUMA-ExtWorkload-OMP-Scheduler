#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <sched_comm.h>

#define PRINT_SIG SIGUSR1
#define STOP_SIG SIGINT

volatile int exit_flag = 0;
omp_numa_t* shmem = NULL;

void parse_args(int argc, char** argv)
{
	//TODO
}

void print_info(int sig)
{
	if(exit_flag)
		return;

	assert(shmem != NULL);
	char str[512];
	char tmp[16];

	snprintf(str, sizeof(str), "OpenMP task information:\n");
	for(int i = 0; i < num_nodes(shmem); i++)
	{
		snprintf(tmp, sizeof(tmp), "\t[%d] %u\n",
			i, num_tasks(shmem, i, FAST_CHECK));
		strcat(str, tmp);
	}

	printf("%s\n", str);
}

void cleanup(int sig)
{
	exit_flag = 1;
	shutdown_omp_numa(shmem);
}

int setup_signals()
{
	struct sigaction print;
	struct sigaction exit;

	// Register handler for printing information
	print.sa_handler = print_info;
	print.sa_flags = 0;
	sigemptyset(&print.sa_mask);
	if(sigaction(PRINT_SIG, &print, NULL) < )
	{
		perror("Could not register signal handler for printing information");
		return -1;
	}

	// Register handler for cleaning up & exiting
	exit.sa_handler = cleanup;
	exit.sa_flags = 0;
	sigemptyset(&exit.sa_mask);
	if(sigaction(PRINT_SIG, &exit, NULL) < )
	{
		perror("Could not register signal handler for exiting");
		return -1;
	}

	return 0;
}

int main(int argc, char** argv)
{
	parse_args(argc, argv);
	shmem = initialize_omp_numa();
	setup_signals();

	while(!exit_flag) {
		pause();
	}

	return 0;
}

