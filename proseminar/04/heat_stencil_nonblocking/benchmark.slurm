#!/bin/bash

# Compare the non-blocking 1D and 2D implementations for several fixed
# problem sizes and MPI process counts. The global problem size is fixed for
# each row of the strong-scaling experiment.

#SBATCH --job-name=heat-stencil-nb
#SBATCH --output=heat_stencil_nonblocking_%j.log
#SBATCH --ntasks=96
#SBATCH --exclusive

set -euo pipefail

make

RANKS=(1 4 16 32 64 96)
PROBLEM_SIZES=(256 512 768)
RESULTS="heat_stencil_nonblocking_${SLURM_JOB_ID:-manual}.csv"

printf "stencil,N,ranks,timesteps,wall_time_seconds,verification\n" > "$RESULTS"

for stencil in 1D 2D; do
    for N in "${PROBLEM_SIZES[@]}"; do
        # Use the same number of updates per problem size in both dimensions.
        T=$((N * 100))

        for ranks in "${RANKS[@]}"; do
            echo "stencil=$stencil N=$N ranks=$ranks T=$T"

            if [[ "$stencil" == "1D" ]]; then
                executable="./heat_stencil_1D_mpi"
            else
                executable="./heat_stencil_2D_mpi"
            fi

            output=$(mpiexec -n "$ranks" "$executable" "$N" "$T")
            printf "%s\n" "$output"

            wall_time=$(printf "%s\n" "$output" |
                awk '/^Wall time:/ { print $3; exit }')
            verification=$(printf "%s\n" "$output" |
                awk '/^Verification:/ { print $2; exit }')

            if [[ -z "$wall_time" || "$verification" != "OK" ]]; then
                echo "Run failed for stencil=$stencil N=$N ranks=$ranks" >&2
                exit 1
            fi

            printf "%s,%d,%d,%d,%s,%s\n" \
                "$stencil" "$N" "$ranks" "$T" "$wall_time" "$verification" \
                >> "$RESULTS"
        done
    done
done

echo "Results written to $RESULTS"
