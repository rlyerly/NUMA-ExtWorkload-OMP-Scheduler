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
SHMEM_HOME="`readlink -f ../../utils`"
RESULTS="$NPB_SCRIPTS/results_!!CONFIG!!_`date +%F_%R`_$$"

###############################################################################
## Configuration & flags
###############################################################################

EXIT=0

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

function get_time {
	echo `cat $1 | grep "Time in seconds =" | \
		sed 's/ Time in seconds =\s\+\([0-9]\+\.[0-9]\+\)/\1/g'`
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
## NUMA
###############################################################################

NUMA_NODES=""

function get_numa_topology {
	local num_nodes=`$NPB_BIN/numa.U.x | grep "Number of NUMA nodes" | \
		sed 's/Number of NUMA nodes: \([0-9][0-9]*\)/\1/g'`
	num_nodes=`expr $num_nodes - 1`
	NUMA_NODES="`seq 0 $num_nodes`"
}

function disable_numa {
	if [ `sysctl -a 2>/dev/null | grep "numa_balancing " | awk '{print $3}'` -eq 1 ]; then
		sudo sysctl kernel.numa_balancing=0 > /dev/null 2>&1 || error "could not disable NUMA!"
	fi
}

function enable_numa {
	if [ `sysctl -a 2>/dev/null | grep "numa_balancing " | awk '{print $3}'` -eq 0 ]; then
		sudo sysctl kernel.numa_balancing=1 > /dev/null 2>&1 || error "could not enable NUMA!"
	fi
}

###############################################################################
## Shared memory
###############################################################################

SHMEM_OUTPUT="/dev/null"

function shared_mem_init {
	$SHMEM_HOME/omp-numa-ctrl start -o $SHMEM_OUTPUT || error "could not start shared memory shepherd!"
}

function shared_mem_info {
	$SHMEM_HOME/omp-numa-ctrl info || warn "could not get shared memory information!"
}

function shared_mem_clear {
	$SHMEM_HOME/omp-numa-ctrl clear || error "could not clear shared memory!"
}

function shared_mem_shutdown {
	$SHMEM_HOME/omp-numa-ctrl stop || warn "could not stop shared memory shepherd!"
}

###############################################################################
## Initialization & teardown
###############################################################################

function shutdown {
	if [ $EXIT -eq 1 ]; then
		return
	else
		# Perform shutdown
		echo -ne "\nShutting down experiments..."

		EXIT=1
		enable_numa
		shared_mem_shutdown

		echo -e "finished!\n"
	fi
}

function initialize {
	echo -n "Initializing experimental setup..."

	# Setup results directory & signal traps
	RESULTS=${RESULTS/!!CONFIG!!/$1} && mkdir -p $RESULTS \
		|| error "couldn't make results directory"
	trap shutdown SIGINT

	# Toggle NUMA
	get_numa_topology
	case $2 in
		no-numa) disable_numa ;;
		*) enable_numa ;;
	esac

	# Setup shared memory
	if [ "$3" != "" ]; then
		SHMEM_OUTPUT=$RESULTS/$3
	fi
	shared_mem_init

	echo -e "finished!"
}

###############################################################################
## Benchmark execution
###############################################################################

function run_thread_configured_bench {
	local cur_bench=$1
	local cur_threads=$2
	local cur_iteration=$3
	local log_file=$4

	if [ "$log_file" != "/dev/null" ]; then
		echo -n " +++ [$cur_iteration] $cur_bench ($5, $cur_threads) -> "
	fi

	OMP_NUM_THREADS=$cur_threads $NPB_BIN/$cur_bench > $log_file

	if [ $? -eq 0 ] && [ "$log_file" != "/dev/null" ]; then
		echo `get_time $log_file`
	else
		echo "could not execute!"
	fi
}

function run_numa_configured_bench {
	local cur_bench=$1
	local cur_cpu_node=$2
	local cur_mem_node=$3
	local cur_iteration=$4
	echo -n " +++ [$cur_iteration] $cur_bench ($cur_cpu_node, $cur_mem_node) -> "

	local log_file=$RESULTS/${cur_bench}-${cur_cpu_node}-${cur_mem_node}-${cur_iteration}.log
	NUMA_CPU_NODES=$cur_cpu_node NUMA_MEM_NODES=$cur_mem_node $NPB_BIN/$cur_bench > $log_file

	if [ $? -eq 0 ]; then
		echo `get_time $log_file`
	else
		echo "could not execute!"
	fi
}

function run_bench {
	local cur_bench=$1
	local cur_iteration=$2
	local log_file=$3

	if [ "$log_file" != "/dev/null" ]; then
		echo -n " +++ [$cur_iteration] $cur_bench -> "
		echo "remove me!"
	fi

	$NPB_BIN/$cur_bench > $log_file

	if [ $? -eq 0 ] && [ "$log_file" != "/dev/null" ]; then
		echo `get_time $log_file`
	else
		echo "could not execute!"
	fi
}

