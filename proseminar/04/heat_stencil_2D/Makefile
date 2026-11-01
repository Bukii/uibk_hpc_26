CC = gcc
MPICC = mpicc
CFLAGS = -O2 -std=gnu11 -Wall -Wextra -pedantic

.PHONY: all clean run mpi

all: heat_stencil_2D

heat_stencil_2D: heat_stencil_2D.c
	@$(CC) $(CFLAGS) $< -o $@

mpi: heat_stencil_2D_mpi

heat_stencil_2D_mpi: heat_stencil_2D_mpi.c
	@$(MPICC) $(CFLAGS) $< -o $@

clean:
	@rm -f heat_stencil_2D heat_stencil_2D_mpi

run: all
	@echo "Sequential:"
	@./heat_stencil_2D
