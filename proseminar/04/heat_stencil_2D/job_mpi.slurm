#!/bin/bash

#SBATCH --output=heat_stencil_2D_mpi.log
#SBATCH --ntasks=96
#SBATCH --exclusive

N=768
T=76800

for ranks in 1 4 16 32 64 96; do
    echo "MPI ranks: $ranks"
    mpiexec -n "$ranks" ./heat_stencil_2D_mpi "$N" "$T"
done
