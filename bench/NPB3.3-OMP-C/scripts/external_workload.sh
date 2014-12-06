#!/bin/bash

source bench_harness.sh

###############################################################################
## Config
###############################################################################

NUM_ITERATIONS=50
BENCH_SIZE="medium"
OMP_NUMA_AWARE_MAPPING=0
EXTERNAL_WORKLOADS="1 2 4 8 16"
BREATHER=0

###############################################################################
## Helper functions
###############################################################################

function print_help {
	echo "external_workload.sh - run external workload experiments"
	echo
	echo "Usage: external_workload.sh [ OPTIONS ]"
	echo "Options:"
	echo -e "\t-h/--help : print help & exit"
	echo -e "\t-i <number> : number of iterations to run (default is $NUM_ITERATIONS)"
	echo -e "\t-s <size>   : benchmark size (valid values are S, W, A, B, C, medium & all)"
	echo -e "\t              default is $BENCH_SIZE"
	echo -e "\t-n          : enable NUMA-aware mapping decisions* (default is $OMP_NUMA_AWARE_MAPPING"
	echo
	echo "*Not yet implemented!"
	exit 0
}

###############################################################################
## Run benchmarks
###############################################################################

function run_workload_launcher {
	local bench_seq="$1"

	# Generate long list
	for i in `seq 5`; do
		bench_seq="$bench_seq $bench_seq"
	done

	# Run randomization of list
	for bench in `shuf -e $bench_seq`; do
		run_bench $bench -1 /dev/null
	done
}

function run_external_workload {
	header "EXTERNAL WORKLOAD EXPERIMENTS"

	# Select benchmark size
	case "$BENCH_SIZE" in
		medium) local benches="$MED_BENCH" ;;
		all) local benches="`gen_all_benches`" ;;
		S | W | A | B | C) local benches="`gen_benches $BENCH_SIZE`" ;;
		*) error "Unknown bench size $BENCH_SIZE" ;;
	esac

	# Iterate through different external workload cases
	for external_workload in $EXTERNAL_WORKLOADS; do
		for bench in $benches; do
			if [ $EXIT -eq 1 ]; then
				break;
			fi

			# Start background launchers
			for i in `seq 2 $external_workload`; do
				run_workload_launcher "$benches" &
			done
			sleep $BREATHER # Wait for the external workload to get moving

			# Run current bench
			for i in `seq $NUM_ITERATIONS`; do
				run_bench $bench $i $RESULTS/${bench}-${external_workload}-${iteration}.log
			done

			# Kill external workload
			pkill -TERM -P $$
			sleep $BREATHER # Wait for external workload to finish
			shared_mem_clear
		done
	done
}

###############################################################################
## Driver code
###############################################################################

while [ "$1" != "" ]; do
	case "$1" in
		-h | --help) print_help ;;
		-i) NUM_ITERATIONS=$2; shift ;;
		-s) BENCH_SIZE=$2; shift ;;
		-n) 
	esac
	shift
done

initialize external_workload numa
time run_external_workload
shutdown

