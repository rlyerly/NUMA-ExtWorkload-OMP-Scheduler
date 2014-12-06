SHELL=/bin/sh
BENCHMARK=numa
BENCHMARKU=NUMA

include ../config/make.def
include ../sys/make.common

OBJS = numa_info.o \
       ${COMMON}/c_print_results.o \
       ${COMMON}/c_timers.o \
       ${COMMON}/c_wtime.o


${PROGRAM}: config ${OBJS}
	${CLINK} ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${C_LIB} -lnuma

.c.o:
	${CCOMPILE} $<

numa_info.o: numa_info.c  npbparams.h


clean:
	- rm -f *.o *~ mputil*
	- rm -f npbparams.h core
	- if [ -d rii_files ]; then rm -r rii_files; fi
