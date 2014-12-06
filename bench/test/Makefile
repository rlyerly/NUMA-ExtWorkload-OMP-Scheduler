OMP_DIR := $(shell readlink -f ../../intelomp/exports)
OMP_INCLUDE := $(OMP_DIR)/common.deb/include
OMP_LIB := $(OMP_DIR)/lin_32e.deb/lib

CC := gcc
COMMON_FLAGS := -O3 -Wall -g
CFLAGS := $(COMMON_FLAGS) -fopenmp -I$(OMP_INCLUDE)
LDFLAGS := $(COMMON_FLAGS) -L$(OMP_LIB) -Wl,-rpath,$(OMP_LIB)
LIBS := -liomp5 -lpthread -lm

VEC_ADD_SRC := vec_add.c
VEC_ADD_OBJ := $(VEC_ADD_SRC:.c=.o)

SHMEM_SRC := shmem_test.c
SHMEM_OBJ := $(SHMEM_SRC:.c=.o)

all: vec_add shmem_test

%.o: %.c
	$(CC) $(CFLAGS) -c $<

vec_add: $(VEC_ADD_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(VEC_ADD_OBJ) $(LIBS)

shmem_test: $(SHMEM_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(SHMEM_OBJ) $(LIBS)

clean:
	rm -f vec_add $(VEC_ADD_OBJ) shmem_test $(SHMEM_OBJ)

.PHONY: clean
