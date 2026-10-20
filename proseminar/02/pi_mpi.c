#include <errno.h>
#include <inttypes.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	// Initialize MPI and make argc/argv available to the MPI runtime.
	MPI_Init(&argc, &argv);

	int rank;
	int size;
	// Get this process's rank and the total number of processes.
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	// Usage: ./pi_mpi SAMPLES
	// SAMPLES: number of samples to use for the Monte Carlo simulation
	// If nothing is specified
	if (argc != 2) {
		if (rank == 0)
			fprintf(stderr, "Usage: %s SAMPLES\n", argv[0]);
		// All ranks must leave MPI collectively, including on an error path
		MPI_Finalize();
		return EXIT_FAILURE;
	}

	char *end = NULL;
	// Text to unsigned long
	uint64_t samples = strtoull(argv[1], &end, 10);
	// Check for errors in conversion and if samples is positive
	if (end == argv[1] || *end != '\0' || samples == 0) {
		if (rank == 0)
			fprintf(stderr, "SAMPLES must be a positive integer\n");
		// Finalize MPI before returning because MPI was already initialized
		MPI_Finalize();
		return EXIT_FAILURE;
	}

	uint64_t points_inside = 0;
	uint64_t total_points = samples;
	uint64_t points_per_rank = samples / (uint64_t)size;
	uint64_t extra_points = samples % (uint64_t)size;
	uint64_t local_points = points_per_rank + ((uint64_t)rank < extra_points); // giga smart
	uint64_t local_points_inside = 0;
	unsigned int seed = 12345 + (unsigned int)rank;
	// Save start time
	double start = MPI_Wtime();

	for (uint64_t i = 0; i < local_points; ++i) {
		long double x = (long double)rand_r(&seed) / (long double)RAND_MAX;
		long double y = (long double)rand_r(&seed) / (long double)RAND_MAX;
		
		if (x * x + y * y <= 1.0L)
			++local_points_inside;
	}

	// Sum the local counts on rank 0 to obtain the global result.
	// points_inside is the dest
	// 1: reduce one value
	// 0: rank 0 is the root that receives the result
	MPI_Reduce(&local_points_inside, &points_inside, 1, MPI_UINT64_T,
		       MPI_SUM, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		// Only rank 0 owns the result
		double elapsed = MPI_Wtime() - start;
		long double pi = 4 * (long double)points_inside / (long double)total_points;
		printf("pi      = %.4Lf\n", pi);
		printf("time    = %.6f seconds\n", elapsed);
	}

	// Shut down the MPI environment after all communication is complete
	MPI_Finalize();
	return EXIT_SUCCESS;
}
