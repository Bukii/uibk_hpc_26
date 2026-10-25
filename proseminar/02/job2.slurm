#!/bin/bash

#SBATCH --job-name=halleluja
#SBATCH --output=output_diff.log
#SBATCH --ntasks=8

./pi_seq 1000000000

mpiexec -n 1 ./pi_mpi 1000000000