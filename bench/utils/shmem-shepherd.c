#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include <sched_comm.h>

#define PRINT_SIG SIGUSR1
#define CLEAR_SIG SIGUSR2
#define STOP_SIG SIGINT

volatile int exit_flag = 0;
omp_numa_t* ipc_handle = NULL;

void parse_args(int argc, char** argv)
{
	//TODO
}

void print_info(int sig)
{
	if(exit_flag)
		return;

	assert(ipc_handle != NULL);
	char str[512];
	char tmp[16];

	snprintf(str, sizeof(str), "OpenMP task information:\n");
	int i;
	for(i = 0; i < omp_numa_num_nodes(ipc_handle); i++)
	{
		snprintf(tmp, sizeof(tmp), "\t[%d] %u\n",
			i, omp_numa_num_tasks(ipc_handle, i, FAST_CHECK));
		strcat(str, tmp);
	}

	printf("%s\n", str);
}

void clear_counters(int sig)
{
	if(exit_flag)
		return;

	assert(ipc_handle != NULL);
	omp_numa_clear_counters(ipc_handle);
}

void cleanup(int sig)
{
	exit_flag = 1;
	omp_numa_shutdown(ipc_handle, SHEPHERD);
}

int setup_signals()
{
	struct sigaction print;
	struct sigaction clear;
	struct sigaction exit;

	// Register handler for printing information
	print.sa_handler = print_info;
	print.sa_flags = 0;
	sigemptyset(&print.sa_mask);
	if(sigaction(PRINT_SIG, &print, NULL) == -1)
	{
		perror("Could not register signal handler for printing information");
		return -1;
	}

	clear.sa_handler = clear_counters;
	clear.sa_flags = 0;
	sigemptyset(&clear.sa_mask);
	if(sigaction(CLEAR_SIG, &clear, NULL) == -1)
	{
		perror("Could not register signal handler for exiting");
		return -1;
	}

	// Register handler for cleaning up & exiting
	exit.sa_handler = cleanup;
	exit.sa_flags = 0;
	sigemptyset(&exit.sa_mask);
	if(sigaction(STOP_SIG, &exit, NULL) == -1)
	{
		perror("Could not register signal handler for exiting");
		return -1;
	}

	return 0;
}

int main(int argc, char** argv)
{
	parse_args(argc, argv);
	ipc_handle = omp_numa_initialize(SHEPHERD);
	setup_signals();

	while(!exit_flag) {
		pause();
	}

	return 0;
}

