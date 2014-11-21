#!/bin/bash

source bench_harness.sh

###############################################################################
## Config
###############################################################################

NUM_ITERATIONS=5
BENCH_SIZE="medium"

###############################################################################
## Helper functions
###############################################################################

function print_help {
	echo "numa_motivation.sh - generate numbers for motivating NUMA-aware scheduling"
	echo
	echo "Usage: numa_motivation.sh [ OPTIONS ]"
	echo "Options:"
	echo -e "\t-h/--help : print help & exit"
	echo -e "\t-i <number> : number of iterations to run"
	echo -e "\t-s <size>   : benchmark size (valid values are S, W, A, B, C, medium & all)"
	echo -e "\t              default is $BENCH_SIZE"
	exit 0
}

###############################################################################
## Run benchmarks
###############################################################################

function run_numa_motivation_bench {
	local cpu_node=$1
	local mem_node=$2

	# Select benchmark size
	case "$BENCH_SIZE" in
		medium) local benches="$MED_BENCH" ;;
		all) local benches="`gen_all_benches`" ;;
		S | W | A | B | C) local benches="`gen_benches $BENCH_SIZE`" ;;
		*) error "Unknown bench size $BENCH_SIZE" ;;
	esac

	# Run benchmarks
	for bench in $benches; do
		for iteration in `seq 1 $NUM_ITERATIONS`; do
			run_configured_bench $bench $cpu_node $mem_node $iteration
		done
	done
}

function run_numa_motivation {
	header "NUMA MOTIVATIONAL EXPERIMENTS"

	for cpu in $NUMA_NODES; do
		for mem in $NUMA_NODES; do
			run_numa_motivation_bench $cpu $mem
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
	esac
	shift
done

initialize "numa_motivation"
time run_numa_motivation
shutdown

