#!/bin/bash

#
# To be included by other scripts, does not execute anything on its own!
#

###############################################################################
## Locations
###############################################################################

NPB_HOME="`readlink -f ../`"
NPB_BIN="$NPB_HOME/bin"
NPB_SCRIPTS="$NPB_HOME/scripts"
RESULTS="$NPB_SCRIPTS/results_!!CONFIG!!_`date +%F_%R`_$$"

###############################################################################
## Helpers
###############################################################################

function header {
	local stars=""
	for i in `seq 1 ${#1}`; do
		stars="${stars}="
	done

	echo -e "\n$stars"
	echo $1
	echo -e "$stars\n"
}

function warn {
	echo "WARNING: $1"
}

function error {
	echo "ERROR: $1"
	exit 1
}

###############################################################################
## Benchmarks & size variations
###############################################################################

BENCH="bt cg dc ep ft is lu mg sp ua"
SIZES="S W A B C"

# Medium-sized benches (10s-30s runtime on 8-core Opteron 6376)
MED_BENCH="bt.A.x cg.B.x dc.W.x ep.A.x ft.B.x \
					 is.C.x lu.A.x mg.C.x sp.A.x ua.A.x"

function gen_benches {
	local tmp=""
	for bench in $BENCH; do
		tmp="$tmp ${bench}.$1.x"
	done
	echo $tmp
}

function gen_all_benches {
	local tmp=""
	for size in $SIZES; do
		tmp="$tmp `gen_benches $size`"
	done
}

###############################################################################
## NUMA Information
###############################################################################

NUMA_NODES=""

function get_numa_topology {
	local num_nodes=`$NPB_BIN/numa.U.x | grep "Number of NUMA nodes" | \
		sed 's/Number of NUMA nodes: \([0-9][0-9]*\)/\1/g'`
	num_nodes=`expr $num_nodes - 1`
	NUMA_NODES="`seq 0 $num_nodes`"
}

###############################################################################
## Initialization & teardown
###############################################################################

function initialize {
	echo -n "Initializing experimental setup..."

	RESULTS=${RESULTS/!!CONFIG!!/$1} && mkdir -p $RESULTS \
		|| error "couldn't make results directory"
	get_numa_topology
	# TODO setup shmem

	echo -e "finished!"
}

function shutdown {
	echo -ne "\nShutting down experiments..."

	# TODO?

	echo -e "finished!\n"
}

###############################################################################
## Benchmark execution
###############################################################################

function run_configured_bench {
	local cur_bench=$1
	local cur_cpu_node=$2
	local cur_mem_node=$3
	local cur_iteration=$4
	echo -n " +++ [$cur_iteration] $cur_bench ($cur_cpu_node, $cur_mem_node) -> "

	local log_file=$RESULTS/${cur_bench}-${cur_cpu_node}-${cur_mem_node}-${cur_iteration}.log
	NUMA_CPU_NODES=$cur_cpu_node NUMA_MEM_NODES=$cur_mem_node $NPB_BIN/$cur_bench > $log_file

	if [ $? -eq 0 ]; then
		echo `cat $log_file | grep "Time in seconds =" | \
			sed 's/ Time in seconds =\s\+\([0-9]\+\.[0-9]\+\)/\1/g'`
	else
		echo "could not execute!"
	fi
}

