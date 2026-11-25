#!/bin/bash

# Searches the largest particle count that finishes 10000 timesteps in under
# 10 seconds on 96 ranks, for each mode and theta: the count is doubled until a
# run is too slow and then bisected until the interval is about 2% wide.
# A count passes if the fastest of at most REPS runs is below the limit.
#
# Submit from this directory. Override defaults like this:
#   sbatch --export=ALL,MODES="put",THETAS="1.0" max_particles.slurm

#SBATCH --partition=lva
#SBATCH --job-name=nbody-max
#SBATCH --output=nbody_max_%j.log
#SBATCH --ntasks=96
#SBATCH --exclusive

set -euo pipefail

MODES="${MODES:-allgatherv put}"
THETAS="${THETAS:-0 0.5 1.0}"
RANKS="${RANKS:-96}"
TIMESTEPS="${TIMESTEPS:-10000}"
LIMIT="${LIMIT:-10}"                     # Seconds.
REPS="${REPS:-3}"
RESULTS="nbody_max_${SLURM_JOB_ID:-manual}.csv"

make -B

# Succeeds if n particles stay below the time limit.
passes() {
    local mode=$1 theta=$2 n=$3 rep line time status
    for rep in $(seq "$REPS"); do
        status=0
        # Runs far above the limit are cut off to save cluster time.
        line=$(timeout $((3 * LIMIT)) mpiexec -n "$RANKS" --map-by core --bind-to core \
               ./nbody_rma "$n" "$TIMESTEPS" "$theta" "$mode" -) || status=$?
        if [[ $status -eq 124 ]]; then
            echo "$mode,$theta,$RANKS,$TIMESTEPS,$n,timeout,0" >> "$RESULTS"
            return 1
        elif [[ $status -ne 0 ]]; then
            echo "run failed with status $status (mode=$mode theta=$theta n=$n)" >&2
            exit "$status"
        fi
        echo "rep=$rep $line"
        time=$(echo "$line" | sed 's/.* time=\([0-9.]*\).*/\1/')
        if awk -v t="$time" -v limit="$LIMIT" 'BEGIN { exit !(t < limit) }'; then
            echo "$mode,$theta,$RANKS,$TIMESTEPS,$n,$time,1" >> "$RESULTS"
            return 0
        fi
        echo "$mode,$theta,$RANKS,$TIMESTEPS,$n,$time,0" >> "$RESULTS"
    done
    return 1
}

echo "mode,theta,ranks,timesteps,particles,time,passed" > "$RESULTS"
summary=""
for mode in $MODES; do
    for theta in $THETAS; do
        low=0                            # Largest count that passed.
        high=$RANKS                      # The program needs at least one per rank.
        while passes "$mode" "$theta" "$high"; do
            low=$high
            high=$((2 * high))
        done
        # Nothing to bisect if even the smallest count is too slow.
        while ((low > 0 && high - low > 1 && high - low > low / 50)); do
            mid=$(((low + high) / 2))
            if passes "$mode" "$theta" "$mid"; then
                low=$mid
            else
                high=$mid
            fi
        done
        summary+="mode=$mode theta=$theta max_particles=$low"$'\n'
    done
done

printf '\n%s' "$summary"
echo "All probes written to $RESULTS"
