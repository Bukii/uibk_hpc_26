#!/bin/bash

# Strong-scaling benchmark: two-sided vs. one-sided communication.
#
# Speedup and efficiency of both modes use the same baseline for each particle
# count and theta: the 1-rank allgatherv run, which does not communicate.
# LCC3 has 12 cores per node, so 96 ranks = 8 nodes.
#
# Submit from this directory. Override defaults like this:
#   sbatch --export=ALL,PARTICLES="5000",RANKS="1 96",REPS=1 benchmark.slurm

#SBATCH --partition=lva
#SBATCH --job-name=nbody-rma
#SBATCH --output=nbody_rma_%j.log
#SBATCH --ntasks=96
#SBATCH --exclusive

set -euo pipefail

PARTICLES="${PARTICLES:-1000 2500 5000 10000}"
THETAS="${THETAS:-0 0.5 1.0}"
TIMESTEPS="${TIMESTEPS:-100}"
RANKS="${RANKS:-1 2 4 6 12 24 48 96}"
REPS="${REPS:-3}"                        # The fastest of REPS runs is reported.
RESULTS="nbody_rma_${SLURM_JOB_ID:-manual}.csv"

# Compile on the compute node so -march=native matches the hardware.
make -B

declare -A CACHE   # "mode ranks particles theta" -> values of the fastest run.

# Sets BEST to "mode,ranks,particles,timesteps,theta,time,build,force,comm".
best_run() {
    local mode=$1 ranks=$2 n=$3 theta=$4
    local key="$mode $ranks $n $theta" rep line values
    BEST="${CACHE[$key]-}"
    if [[ -n "$BEST" ]]; then
        return
    fi
    for rep in $(seq "$REPS"); do
        # Fill nodes core by core and pin each rank for stable timings.
        # "-" disables trajectory output, so only the simulation is timed.
        line=$(mpiexec -n "$ranks" --map-by core --bind-to core \
               ./nbody_rma "$n" "$TIMESTEPS" "$theta" "$mode" -)
        echo "rep=$rep $line"
        values=$(echo "$line" | sed 's/[a-z]*=//g; s/ /,/g')
        if [[ -z "$BEST" ]] || awk -v a="$values" -v b="$BEST" \
            'BEGIN { split(a, x, ","); split(b, y, ","); exit !(x[6] + 0 < y[6] + 0) }'; then
            BEST="$values"
        fi
    done
    CACHE[$key]="$BEST"
}

echo "mode,theta,particles,ranks,timesteps,time,build,force,comm,speedup,efficiency" > "$RESULTS"

for n in $PARTICLES; do
    for theta in $THETAS; do
        best_run allgatherv 1 "$n" "$theta"
        base=$(echo "$BEST" | cut -d, -f6)
        for mode in allgatherv put; do
            for ranks in $RANKS; do
                best_run "$mode" "$ranks" "$n" "$theta"
                echo "$BEST" | awk -F, -v base="$base" \
                    'BEGIN { OFS = "," }
                     { s = base / $6
                       print $1, $5, $3, $2, $4, $6, $7, $8, $9, s, s / $2 }' >> "$RESULTS"
            done
        done
    done
done

echo "Results written to $RESULTS"
