BIN := shmem-shepherd

OMP_DIR := $(shell readlink -f ../../intelomp/exports)
OMP_INCLUDE := $(OMP_DIR)/common.deb/include
OMP_LIB := $(OMP_DIR)/lin_32e.deb/lib

CC := gcc
CFLAGS := -O3 -Wall -g -I$(OMP_INCLUDE) -L$(OMP_LIB) -Wl,-rpath,$(OMP_LIB)
LIBS := -liomp5

SHEPHERD_SRC := shmem-shepherd.c

$(BIN): $(SHEPHERD_SRC)
	$(CC) $(CFLAGS) -o $@ $(SHEPHERD_SRC) $(LIBS)

clean:
	rm -f $(BIN)

.PHONY: clean
