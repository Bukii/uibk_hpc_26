#!/bin/bash

#SBATCH --output=output.log
#SBATCH --exclusive

N=768
T=76800

for ranks in 1 4 16 32 64 96; do
    echo "MPI ranks: $ranks"
    mpiexec -n "$ranks" ./heat_stencil_2D_mpi "$N" "$T"
done