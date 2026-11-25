MPICC = mpicc
CFLAGS = -O3 -march=native -std=gnu11 -Wall -Wextra -Werror
LDLIBS = -lm

.PHONY: all clean

all: nbody_rma

nbody_rma: nbody_rma.c
	$(MPICC) $(CFLAGS) $< -o $@ $(LDLIBS)

clean:
	rm -f nbody_rma
