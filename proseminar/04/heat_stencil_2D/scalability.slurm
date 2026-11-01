#!/bin/bash

# Strong scaling is used here because the global problem size remains fixed
# while the number of MPI processes increases. This measures how efficiently
# the same calculation is divided among more processes.

#SBATCH --output=heat_stencil_scaling.log
#SBATCH --ntasks=96
#SBATCH --exclusive

RANKS=(1 4 16 32 64 96)
PROBLEM_SIZES=(256 512 768)
RESULTS="heat_stencil_scaling.csv"

printf "N,ranks,timesteps,wall_time_seconds,speedup,efficiency\n" > "$RESULTS"

for N in "${PROBLEM_SIZES[@]}"; do
    T=$((N * 100))
    baseline=""

    for ranks in "${RANKS[@]}"; do
        output=$(mpiexec -n "$ranks" ./heat_stencil_2D_mpi "$N" "$T")
        printf "%s\n" "$output"

        wall_time=$(printf "%s\n" "$output" |
            awk '/^Wall time:/ { print $3; exit }')
        verification=$(printf "%s\n" "$output" |
            awk '/^Verification:/ { print $2; exit }')

        if [[ -z "$wall_time" || "$verification" != "OK" ]]; then
            echo "MPI run failed for N=$N and ranks=$ranks" >&2
            exit 1
        fi

        if [[ -z "$baseline" ]]; then
            baseline="$wall_time"
        fi

        awk -v n="$N" -v p="$ranks" -v t="$T" \
            -v time="$wall_time" -v base="$baseline" \
            'BEGIN {
              speedup = base / time
              efficiency = speedup / p
              printf "%d,%d,%d,%.9f,%.6f,%.6f\n",
                     n, p, t, time, speedup, efficiency
            }' >> "$RESULTS"
    done
done

echo "Results written to $RESULTS"
