#!/bin/bash

#SBATCH --job-name=halleluja
#SBATCH --output=output.log
#SBATCH --ntasks=96

SAMPLES=1000000000

echo "Sequential:"
./pi_seq "$SAMPLES"

for ranks in 1 6 12 24 48 96; do
    echo "MPI ranks: $ranks"
    mpiexec -n "$ranks" ./pi_mpi "$SAMPLES"
done