#include <errno.h>
#include <inttypes.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	MPI_Init(&argc, &argv);

	int rank;
	int size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	// Usage: ./pi_mpi SAMPLES
	// SAMPLES: number of samples to use for the Monte Carlo simulation
	// If nothing is specified
	if (argc != 2) {
		if (rank == 0)
			fprintf(stderr, "Usage: %s SAMPLES\n", argv[0]);
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
		MPI_Finalize();
		return EXIT_FAILURE;
	}

	uint64_t points_inside = 0;
	uint64_t total_points = samples;
	uint64_t points_per_rank = samples / (uint64_t)size;
	uint64_t extra_points = samples % (uint64_t)size;
	uint64_t local_points = points_per_rank + ((uint64_t)rank < extra_points);
	uint64_t local_points_inside = 0;
	unsigned int seed = 12345 + (unsigned int)rank;

	for (uint64_t i = 0; i < local_points; ++i) {
		long double x = (long double)rand_r(&seed) / (long double)RAND_MAX;
		long double y = (long double)rand_r(&seed) / (long double)RAND_MAX;
		
		if (x * x + y * y <= 1.0L)
			++local_points_inside;
	}

	MPI_Reduce(&local_points_inside, &points_inside, 1, MPI_UINT64_T,
		       MPI_SUM, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		long double pi = 4 * (long double)points_inside / (long double)total_points;
		printf("pi      = %.4Lf\n", pi);
	}

	MPI_Finalize();
	return EXIT_SUCCESS;
}
