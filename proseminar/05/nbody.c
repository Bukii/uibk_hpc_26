#include <errno.h>  // Provides errno for detecting conversion errors.
#include <math.h>   // Provides sqrt for calculating distances.
#include <stdio.h>  // Provides file and console input/output functions.
#include <stdint.h> // Provides SIZE_MAX for overflow checks.
#include <stdlib.h> // Provides memory allocation, conversion, and exit functions.
#include <time.h>   // Provides clock for measuring CPU time.

#define GRAVITATIONAL_CONSTANT 1.0 // Simplified gravitational constant G.
#define DEFAULT_PARTICLES 100       // Number of particles when no argument is given.
#define DEFAULT_TIMESTEPS 100       // Number of simulation steps by default.
#define DEFAULT_OUTPUT "data.dat"   // Default file used for trajectory output.
#define INITIAL_POSITION_RANGE 100.0 // Random coordinates are in [0, 100).

typedef struct {
    double x; // X component of the vector.
    double y; // Y component of the vector.
    double z; // Z component of the vector.
} Vector3;

typedef struct {
    Vector3 position; // Current location of the particle.
    Vector3 velocity; // Current movement per unit of time.
    double mass;      // Particle mass used in the force calculation.
} Particle;

static double random_coordinate(void)
{
    // Scale rand's [0, RAND_MAX] value to the configured coordinate range.
    return INITIAL_POSITION_RANGE * (double) rand() / (double) RAND_MAX;
}

static void initialize_particles(Particle *particles, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        // Give every particle a random starting location.
        particles[i].position = (Vector3) {
            random_coordinate(), random_coordinate(), random_coordinate()
        };
        // Start all particles at rest so only gravity changes their velocity.
        particles[i].velocity = (Vector3) {0.0, 0.0, 0.0};
        // Unit masses keep the example simple while preserving the general formula.
        particles[i].mass = 1.0;
    }
}

static void calculate_forces(const Particle *particles, Vector3 *forces,
                             size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        // Force is accumulated independently for each target particle.
        forces[i] = (Vector3) {0.0, 0.0, 0.0};

        for (size_t j = 0; j < count; ++j) {
            if (i == j) {
                // A particle does not exert a force on itself.
                continue;
            }

            // Vector from particle i toward the particle exerting the force.
            const double dx = particles[j].position.x - particles[i].position.x;
            const double dy = particles[j].position.y - particles[i].position.y;
            const double dz = particles[j].position.z - particles[i].position.z;
            // Squared distance is used in Newton's inverse-square law.
            const double distance_squared = dx * dx + dy * dy + dz * dz;

            if (distance_squared == 0.0) {
                // Avoid division by zero if two particles overlap exactly.
                continue;
            }

            // The square root converts squared distance into distance for normalization.
            const double distance = sqrt(distance_squared);
            // Calculate the scalar gravitational force between the two particles.
            const double force = GRAVITATIONAL_CONSTANT
                               * particles[i].mass * particles[j].mass
                               / distance_squared;

            // Convert scalar force to a vector and add it to particle i's total.
            forces[i].x += force * dx / distance;
            forces[i].y += force * dy / distance;
            forces[i].z += force * dz / distance;
        }
    }
}

static void update_particles(Particle *particles, const Vector3 *forces,
                             size_t count, double dt)
{
    for (size_t i = 0; i < count; ++i) {
        // Acceleration equals force divided by mass; integrate velocity over dt.
        particles[i].velocity.x += forces[i].x / particles[i].mass * dt;
        particles[i].velocity.y += forces[i].y / particles[i].mass * dt;
        particles[i].velocity.z += forces[i].z / particles[i].mass * dt;

        // Move each particle using its newly updated velocity.
        particles[i].position.x += particles[i].velocity.x * dt;
        particles[i].position.y += particles[i].velocity.y * dt;
        particles[i].position.z += particles[i].velocity.z * dt;
    }
}

static void write_positions(FILE *output, const Vector3 *positions,
                            size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        // Write one particle per line in the x y z format expected by gnuplot.
        fprintf(output, "%.10f %.10f %.10f\n",
                positions[i].x, positions[i].y, positions[i].z);
    }
    // Two blank lines separate this timestep from the next animation frame.
    fputs("\n\n", output);
}

static size_t parse_positive_number(const char *text, const char *name)
{
    char *end; // Will point to the first character not consumed by strtoull.
    errno = 0; // Clear a possible error from an earlier library call.
    unsigned long long value = strtoull(text, &end, 10); // Parse base-10 input.

    // Reject empty, non-numeric, zero, overflowing, and partially parsed values.
    if (errno != 0 || *text < '0' || *text > '9' || *end != '\0'
        || value == 0
        || value > (size_t) -1) {
        // Report the invalid argument and terminate before it reaches allocation.
        fprintf(stderr, "%s must be a positive integer: %s\n", name, text);
        exit(EXIT_FAILURE);
    }

    return (size_t) value;
}

int main(int argc, char **argv)
{
    // Read optional command-line values, or use the documented defaults.
    const size_t particle_count = argc > 1
                                ? parse_positive_number(argv[1], "particles")
                                : DEFAULT_PARTICLES;
    const size_t timestep_count = argc > 2
                                ? parse_positive_number(argv[2], "timesteps")
                                : DEFAULT_TIMESTEPS;
    const char *output_name = argc > 3 ? argv[3] : DEFAULT_OUTPUT;

    if (argc > 4) {
        // More than three user arguments indicates an incorrect command format.
        fprintf(stderr, "Usage: %s [particles] [timesteps] [output-file]\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    // Check every multiplication before allocating to prevent size_t overflow.
    if (particle_count > SIZE_MAX / sizeof(Particle)
        || timestep_count > SIZE_MAX / particle_count
        || timestep_count * particle_count > SIZE_MAX / sizeof(Vector3)) {
        // Refuse an allocation size that cannot be represented safely.
        fprintf(stderr, "Requested simulation is too large\n");
        return EXIT_FAILURE;
    }

    // Allocate particle state, per-particle forces, and all trajectory frames.
    Particle *particles = malloc(particle_count * sizeof(*particles));
    Vector3 *forces = malloc(particle_count * sizeof(*forces));
    Vector3 *trajectory = malloc(timestep_count * particle_count
                                 * sizeof(*trajectory));
    if (particles == NULL || forces == NULL || trajectory == NULL) {
        // Clean up any successful partial allocations before returning.
        fprintf(stderr, "Could not allocate memory for %zu particles\n",
                particle_count);
        free(particles);
        free(forces);
        free(trajectory);
        return EXIT_FAILURE;
    }

    // A fixed seed makes runs reproducible for comparison and benchmarking.
    srand(42);
    initialize_particles(particles, particle_count);

    // Open the output before timing so file creation is excluded from the benchmark.
    FILE *output = fopen(output_name, "w");
    if (output == NULL) {
        // perror includes the operating system's reason for the failure.
        perror(output_name);
        free(particles);
        free(forces);
        free(trajectory);
        return EXIT_FAILURE;
    }

    // Start timing only the actual n-body simulation.
    const clock_t start = clock();
    for (size_t timestep = 0; timestep < timestep_count; ++timestep) {
        // Compute all pairwise interactions using the positions at this timestep.
        calculate_forces(particles, forces, particle_count);
        // Apply the forces and advance every particle by dt = 1.
        update_particles(particles, forces, particle_count, 1.0);

        for (size_t i = 0; i < particle_count; ++i) {
            // Save this frame in memory so file I/O does not affect timing.
            trajectory[timestep * particle_count + i] = particles[i].position;
        }
    }
    // Stop timing before writing the saved trajectory to disk.
    const clock_t end = clock();

    for (size_t timestep = 0; timestep < timestep_count; ++timestep) {
        // Write frames only after the simulation has been measured.
        write_positions(output, trajectory + timestep * particle_count,
                        particle_count);
    }

    if (fclose(output) != 0) {
        // Report errors that occur while flushing or closing the output file.
        perror(output_name);
        free(particles);
        free(forces);
        free(trajectory);
        return EXIT_FAILURE;
    }

    // Convert clock ticks to seconds for a human-readable benchmark result.
    printf("Simulated %zu particles for %zu timesteps in %.6f seconds\n",
           particle_count, timestep_count,
           (double) (end - start) / (double) CLOCKS_PER_SEC);

    // Release all heap allocations before exiting successfully.
    free(particles);
    free(forces);
    free(trajectory);
    return EXIT_SUCCESS;
}
