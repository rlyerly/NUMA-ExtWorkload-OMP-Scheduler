BIN := vec_add

OMP_DIR := $(shell readlink -f ../../intelomp/exports)
OMP_INCLUDE := $(OMP_DIR)/common.deb/include
OMP_LIB := $(OMP_DIR)/lin_32e.deb/lib

CC := gcc
COMMON_FLAGS := -O3 -Wall -g
CFLAGS := $(COMMON_FLAGS) -fopenmp -I$(OMP_INCLUDE)
LDFLAGS := $(COMMON_FLAGS) -L$(OMP_LIB) -Wl,-rpath,$(OMP_LIB)
LIBS := -liomp5 -lpthread

SRC := vec_add.c
OBJ := $(SRC:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

$(BIN): $(OBJ) $(OMP_LIB)/libiomp5.so
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)

clean:
	rm -f $(BIN) $(OBJ)

.PHONY: clean
