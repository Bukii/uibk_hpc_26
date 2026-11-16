#!/bin/bash

# Strong-scaling benchmark for Exercise 2: uniform vs. corner initial
# distribution (all velocities start at 0). For each distribution and theta
# (0 = direct, 0.5 and 1.0 = Barnes-Hut) the ranks are swept from 1 to 96.
# LCC3 has 12 cores per node, so 96 ranks = 8 nodes.
#
# Override defaults like this:
#   sbatch --export=ALL,PARTICLES="2500 5000",RANKS="1 96",REPS=1 benchmark_ex2.slurm

#SBATCH --partition=lva
#SBATCH --job-name=nbody-ex2
#SBATCH --output=nbody_ex2_%j.log
#SBATCH --ntasks=96
#SBATCH --exclusive

set -euo pipefail

PARTICLES="${PARTICLES:-5000}"
TIMESTEPS="${TIMESTEPS:-100}"
DISTS="${DISTS:-uniform corner}"
THETAS="${THETAS:-0 0.5 1.0}"
RANKS="${RANKS:-1 2 4 6 12 24 48 96}"    # 1 rank must come first (baseline).
REPS="${REPS:-3}"                        # The fastest of REPS runs is reported.
RESULTS="nbody_ex2_${SLURM_JOB_ID:-manual}.csv"

# Compile on the compute node so -march=native matches the hardware.
mpicc -O3 -march=native -std=c11 -Wall -Wextra -o nbody_mpi_ex2 nbody_mpi_ex2.c -lm

echo "dist,theta,particles,ranks,timesteps,time,build,force,comm,imbalance,speedup,efficiency" > "$RESULTS"

for n in $PARTICLES; do
    for dist in $DISTS; do
        for theta in $THETAS; do
            base=""                       # 1-rank time of this dist/theta.
            for ranks in $RANKS; do
                best=""
                for rep in $(seq "$REPS"); do
                    # Fill nodes core by core and pin each rank for stable timings.
                    # "-" disables trajectory output, so only the simulation is timed.
                    line=$(mpiexec -n "$ranks" --map-by core --bind-to core \
                           ./nbody_mpi_ex2 "$n" "$TIMESTEPS" "$theta" "$dist" -)
                    echo "rep=$rep $line"
                    # "dist=corner ranks=1 ..." -> "corner,1,..."; field 6 is the wall time.
                    values=$(echo "$line" | sed 's/[a-z]*=//g; s/ /,/g')
                    time=$(echo "$values" | cut -d, -f6)
                    if [[ -z "$best" ]] || awk -v a="$time" -v b="$(echo "$best" | cut -d, -f6)" 'BEGIN { exit !(a < b) }'; then
                        best="$values"
                    fi
                done

                time=$(echo "$best" | cut -d, -f6)
                base="${base:-$time}"
                # Program order: dist,ranks,particles,timesteps,theta,time,build,force,comm,imbalance
                # CSV order:     dist,theta,particles,ranks,timesteps,time,build,force,comm,imbalance,speedup,efficiency
                echo "$best" | awk -F, -v base="$base" \
                    'BEGIN { OFS = "," }
                     { s = base / $6
                       print $1, $5, $3, $2, $4, $6, $7, $8, $9, $10, s, s / $2 }' >> "$RESULTS"
            done
        done
    done
done

echo "Results written to $RESULTS"
