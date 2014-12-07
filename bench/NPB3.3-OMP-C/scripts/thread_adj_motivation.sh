#!/bin/bash

source bench_harness.sh

###############################################################################
## Config
###############################################################################

NUM_ITERATIONS=5
BENCH_SIZE="medium"
NUM_PROCS=`cat /proc/cpuinfo | grep processor | wc -l`
NUM_THREADS="$NUM_PROCS `expr $NUM_PROCS / 2` `expr $NUM_PROCS / 4`"

###############################################################################
## Helper functions
###############################################################################

function print_help {
	echo "thread_adj_motivation.sh - generate numbers for motivating dynamic number thread adjustment"
	echo
	echo "Usage: thread_adj_motivation.sh [ OPTIONS ]"
	echo "Options:"
	echo -e "\t-h/--help   : print help & exit"
	echo -e "\t-i <number> : number of iterations to run (default is $NUM_ITERATIONS)"
	echo -e "\t-s <size>   : benchmark size (valid values are S, W, A, B, C, medium & all)"
	echo -e "\t              default is $BENCH_SIZE"
	exit 0
}

###############################################################################
## Run benchmarks
###############################################################################

function corun_thread_adj_motivation_benches {
	run_thread_configured_bench $1 $3 $4 $RESULTS/${1}-${2}-${3}-${4}.log $2 &
	run_thread_configured_bench $2 $3 $4 /dev/null &
	wait
}

function run_thread_adj_motivation {
	header "DYNAMIC THREAD ADJUSTMENT MOTIVATIONAL EXPERIMENTS"

	# Select benchmark size
	case "$BENCH_SIZE" in
		medium) local benches="$MED_BENCH" ;;
		all) local benches="`gen_all_benches`" ;;
		S | W | A | B | C) local benches="`gen_benches $BENCH_SIZE`" ;;
		*) error "Unknown bench size $BENCH_SIZE" ;;
	esac

	# Iterate through possible combinations
	for thread in $NUM_THREADS; do
		for bench_a in $benches; do
			for bench_b in $benches; do
				# DC cannot be co-run with itself!
				if [[ $bench_a =~ dc.* ]] && [[ $bench_b =~ dc.* ]]; then
					continue
				fi

				for iter in `seq $NUM_ITERATIONS`; do
					corun_thread_adj_motivation_benches $bench_a $bench_b $thread $iter
					if [ $EXIT -eq 1 ]; then
						break
					fi
				done
			done
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

initialize thread_adj_motivation no-numa
time run_thread_adj_motivation
shutdown

