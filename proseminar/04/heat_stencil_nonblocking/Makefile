CC = gcc
MPICC = mpicc
CFLAGS = -O2 -std=gnu11 -Wall -Wextra -pedantic

.PHONY: all clean

all: heat_stencil_1D_mpi heat_stencil_2D_mpi

heat_stencil_1D_mpi: heat_stencil_1D_mpi.c
	$(MPICC) $(CFLAGS) $< -o $@

heat_stencil_2D_mpi: heat_stencil_2D_mpi.c
	$(MPICC) $(CFLAGS) $< -o $@

clean:
	rm -f heat_stencil_1D_mpi heat_stencil_2D_mpi
