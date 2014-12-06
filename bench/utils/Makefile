BIN := shmem-shepherd

OMP_DIR := $(shell readlink -f ../../intelomp/exports)
OMP_INCLUDE := $(OMP_DIR)/common.deb/include
OMP_LIB := $(OMP_DIR)/lin_32e.deb/lib

CC := gcc
CFLAGS := -O3 -Wall -g -I$(OMP_INCLUDE) -L$(OMP_LIB) -Wl,-rpath,$(OMP_LIB)
LIBS := -liomp5

SHEPHERD_SRC := shmem-shepherd.c
TEST_SRC := shmem-test.c

$(BIN): $(SHEPHERD_SRC)
	$(CC) $(CFLAGS) -o $@ $(SHEPHERD_SRC) $(LIBS)

test: $(BIN)
	$(CC) $(CFLAGS) -fopenmp -o shmem-test $(TEST_SRC) $(LIBS)

clean:
	rm -f $(BIN) shmem-test

.PHONY: clean test
