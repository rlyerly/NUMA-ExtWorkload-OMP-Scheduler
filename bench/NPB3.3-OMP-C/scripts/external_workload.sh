#!/bin/bash

source bench_harness.sh

###############################################################################
## Config
###############################################################################

NUM_ITERATIONS=50
BENCH_SIZE="medium"
RAW_OPENMP=0
OMP_NUMA_AWARE_MAPPING=0
EXTERNAL_WORKLOADS="1 2 4 8" #16
BREATHER=5

###############################################################################
## Helper functions
###############################################################################

function print_help {
	echo "external_workload.sh - run external workload experiments"
	echo
	echo "Usage: external_workload.sh [ OPTIONS ]"
	echo "Options:"
	echo -e "\t-h/--help   : print help & exit"
	echo -e "\t-i <number> : number of iterations to run (default is $NUM_ITERATIONS)"
	echo -e "\t-s <size>   : benchmark size (valid values are S, W, A, B, C, medium & all)"
	echo -e "\t              default is $BENCH_SIZE"
	echo -e "\t-n          : enable NUMA-aware mapping decisions (default is $OMP_NUMA_AWARE_MAPPING)"
	echo -e "\t-d          : disable context-aware support"
	exit 0
}

function configure {
	if [ $RAW_OPENMP -eq 1 ]; then
		shared_mem_shutdown
	fi

	export OMP_NUMA_AWARE_MAPPING
}

###############################################################################
## Run benchmarks
###############################################################################

function run_workload_launcher {
	local bench_seq="$1"

	# Cut out DC, as it doesn't play nicely with other benchmarks
	bench_seq=${bench_seq/dc.W.x}

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
			if [ $external_workload -ne 1 ]; then
				for i in `seq 2 $external_workload`; do
					run_workload_launcher "$benches" &
				done
				sleep $BREATHER # Wait for the external workload to get moving
			fi

			# Run current bench
			for iter in `seq $NUM_ITERATIONS`; do
				run_bench $bench $iter $RESULTS/${bench}-${external_workload}-${iter}.log
				if [ $EXIT -eq 1 ]; then
					break;
				fi
			done

			# Kill external workload
			if [ $external_workload -ne 1 ]; then
				pkill -TERM -P $$
				sleep $(( 3 * $BREATHER )) # Wait for external workload to finish
																	 # Note: wait a little longer since killing
																	 # the workload launcher doesn't kill the
																	 # application it's currently running
			fi

			if [ $RAW_OPENMP -eq 0 ]; then
#				shared_mem_clear
				shared_mem_shutdown # Just in case...
				shared_mem_init
			fi
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
		-d) RAW_OPENMP=1 ;;
		-n) OMP_NUMA_AWARE_MAPPING=1 ;;
	esac
	shift
done

initialize external_workload numa
configure
time run_external_workload
shutdown

