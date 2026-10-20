#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    // Usage: ./pi_seq SAMPLES
    // SAMPLES: number of samples to use for the Monte Carlo simulation
    // If nothing is specified
	if (argc != 2) {
		fprintf(stderr, "Usage: %s SAMPLES\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *conversion_end = NULL;
    // Text to unsigned long
	uint64_t samples = strtoull(argv[1], &conversion_end, 10);
    // Check for errors in conversion and if samples is positive
	if (conversion_end == argv[1] || *conversion_end != '\0' || samples == 0) {
		fprintf(stderr, "SAMPLES must be a positive integer\n");
		return EXIT_FAILURE;
	}

	uint64_t points_inside = 0;
	uint64_t total_points = samples;
	unsigned int seed = 12345;
	struct timespec start;
	struct timespec end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for (uint64_t i = 0; i < samples; ++i) {
		long double x = (long double)rand_r(&seed) / (long double)RAND_MAX;
		long double y = (long double)rand_r(&seed) / (long double)RAND_MAX;
        
		if (x * x + y * y <= 1.0L)
			++points_inside;
	}

	long double pi = 4 * (long double)points_inside / (long double)total_points;
	clock_gettime(CLOCK_MONOTONIC, &end);
	double elapsed = (double)(end.tv_sec - start.tv_sec) +
	                 (double)(end.tv_nsec - start.tv_nsec) / 1e9;
	printf("pi      = %.4Lf\n", pi);
	printf("time    = %.6f seconds\n", elapsed);
	return EXIT_SUCCESS;
}
