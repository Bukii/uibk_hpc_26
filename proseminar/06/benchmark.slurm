#!/bin/bash

# Strong-scaling benchmark for Exercise 1.
#
# Variants (selected via theta, the 3rd program argument):
#   theta=0    direct O(N^2) summation -> measures the MPI parallelization alone
#   theta=0.5  Barnes-Hut              -> MPI + optimization (standard accuracy)
#   theta=1.0  Barnes-Hut              -> faster, less accurate
# The 1-rank theta=0 run is the Assignment 5 algorithm and serves as the
# sequential baseline. LCC3 has 12 cores per node, so 96 ranks = 8 nodes.
#
# Override defaults like this:
#   sbatch --export=ALL,PARTICLES="5000",RANKS="1 96",REPS=1 benchmark.slurm

#SBATCH --partition=lva
#SBATCH --job-name=nbody-bench
#SBATCH --output=nbody_bench_%j.log
#SBATCH --ntasks=96
#SBATCH --exclusive

set -euo pipefail

PARTICLES="${PARTICLES:-1000 2500 5000 10000}"
TIMESTEPS="${TIMESTEPS:-100}"
THETAS="${THETAS:-0 0.5 1.0}"            # theta=0 must come first (baseline).
RANKS="${RANKS:-1 2 4 6 12 24 48 96}"    # 1 rank must come first (baseline).
REPS="${REPS:-3}"                        # The fastest of REPS runs is reported.
RESULTS="nbody_bench_${SLURM_JOB_ID:-manual}.csv"

# Compile on the compute node so -march=native matches the hardware.
mpicc -O3 -march=native -std=c11 -Wall -Wextra -o nbody_mpi nbody_mpi.c -lm

echo "theta,particles,ranks,timesteps,time,build,force,comm,speedup,efficiency,speedup_vs_direct" > "$RESULTS"

for n in $PARTICLES; do
    direct_base=""                        # 1-rank time with theta=0 for this n.
    for theta in $THETAS; do
        base=""                           # 1-rank time of this variant.
        for ranks in $RANKS; do
            best=""
            for rep in $(seq "$REPS"); do
                # Fill nodes core by core and pin each rank for stable timings.
                # "-" disables trajectory output, so only the simulation is timed.
                line=$(mpiexec -n "$ranks" --map-by core --bind-to core \
                       ./nbody_mpi "$n" "$TIMESTEPS" "$theta" -)
                echo "rep=$rep $line"
                # "ranks=1 ... comm=0.1" -> "1,...,0.1"; field 5 is the wall time.
                values=$(echo "$line" | sed 's/[a-z]*=//g; s/ /,/g')
                time=$(echo "$values" | cut -d, -f5)
                if [[ -z "$best" ]] || awk -v a="$time" -v b="$(echo "$best" | cut -d, -f5)" 'BEGIN { exit !(a < b) }'; then
                    best="$values"
                fi
            done

            time=$(echo "$best" | cut -d, -f5)
            base="${base:-$time}"
            direct_base="${direct_base:-$time}"
            # Reorder to theta,particles,ranks,timesteps,time,build,force,comm
            # and append speedup/efficiency relative to the 1-rank baselines.
            echo "$best" | awk -F, -v base="$base" -v dbase="$direct_base" \
                'BEGIN { OFS = "," }
                 { s = base / $5
                   print $4, $2, $1, $3, $5, $6, $7, $8, s, s / $1, dbase / $5 }' >> "$RESULTS"
        done
    done
done

echo "Results written to $RESULTS"
