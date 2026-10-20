#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    // Usage: ./pi_seq SAMPLES
    // SAMPLES: number of samples to use for the Monte Carlo simulation
    // If nothing is specified
	if (argc != 2) {
		fprintf(stderr, "Usage: %s SAMPLES\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *end = NULL;
    // Text to unsigned long
	uint64_t samples = strtoull(argv[1], &end, 10);
    // Check for errors in conversion and if samples is positive
	if (end == argv[1] || *end != '\0' || samples == 0) {
		fprintf(stderr, "SAMPLES must be a positive integer\n");
		return EXIT_FAILURE;
	}

	uint64_t points_inside = 0;
	uint64_t total_points = samples;
	unsigned int seed = 12345;
	for (uint64_t i = 0; i < samples; ++i) {
		long double x = (long double)rand_r(&seed) / (long double)RAND_MAX;
		long double y = (long double)rand_r(&seed) / (long double)RAND_MAX;
        
		if (x * x + y * y <= 1.0L)
			++points_inside;
	}

	long double pi = 4 * (long double)points_inside / (long double)total_points;
	printf("pi      = %.4Lf\n", pi);
	return EXIT_SUCCESS;
}
