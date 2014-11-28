BIN := shmem-host

OMP_DIR := ../../intelomp/exports
OMP_INCLUDE := $(OMP_DIR)/common/include
OMP_LIB := $(OMP_DIR)/lin_32e/lib

CC := gcc
CFLAGS := -O3 -Wall -g -I$(OMP_INCLUDE) -L$(OMP_LIB) -Wl,-rpath,$(OMP_LIB)
LIBS := -liomp5

SHEPHERD_SRC := shmem-shepherd.c
TEST_SRC := shmem-test.c

$(BIN): $(SHEPHERD_SRC)
	$(CC) $(CFLAGS) -o $@ $(SHEPHERD_SRC) $(LIBS)

test: $(BIN)
	$(CC) $(CFLAGS) -o shmem-test $(TEST_SRC) $(LIBS)

clean:
	rm -f $(BIN) shmem-test

.PHONY: clean test
