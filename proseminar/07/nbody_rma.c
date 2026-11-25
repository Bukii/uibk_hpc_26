// MPI n-body simulation (uniform initial distribution) comparing two-sided and
// one-sided communication. Based on nbody.c (Assignment 6).
//
// Usage: nbody_rma [particles] [timesteps] [theta] [mode] [output-file]
//   theta = 0   exact direct O(N^2) summation
//   theta > 0   Barnes-Hut approximation, O(N log N)
//   output "-"  disables trajectory output (use this for benchmarks)
//
// Modes (how the new positions are shared after every timestep):
//   allgatherv  one MPI_Allgatherv (two-sided baseline)
//   put         one MPI_Put per target rank, synchronized with MPI_Win_fence
//
// Every rank holds all positions and computes the forces for a contiguous block
// of particles. MPI errors are not checked: the default error handler
// MPI_ERRORS_ARE_FATAL already aborts the whole run on any failure.

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define G 1.0              // Simplified gravitational constant.
#define MASS 1.0           // All particles have unit mass.
#define RANGE 100.0        // Initial coordinates are drawn from [0, RANGE].
#define SOFTENING 1.0e-9   // Added to r^2 to avoid division by zero.
#define MAX_DEPTH 64       // Stops splitting for (nearly) identical positions.

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    Vec3 center;    // Geometric center of this cubic cell.
    double half;    // Half of the cell's edge length.
    double mass;    // Total mass of all particles in the cell.
    Vec3 com;       // Center of mass (a mass-weighted sum while building).
    int particle;   // Particle stored in a leaf, or -1.
    int child[8];   // Indices of the 8 sub-cells; child[0] == -1 for leaves.
} Node;

typedef struct {
    Node *nodes;    // All nodes in one array; node 0 is the root.
    int count;      // Nodes used in the current timestep.
    int capacity;   // Allocated nodes (memory is reused between timesteps).
} Tree;

static int new_node(Tree *t, Vec3 center, double half)
{
    if (t->count == t->capacity) {
        // Grow geometrically; after the first timesteps this rarely happens.
        t->capacity = t->capacity ? 2 * t->capacity : 1024;
        t->nodes = realloc(t->nodes, (size_t) t->capacity * sizeof(Node));
        if (t->nodes == NULL) {
            fprintf(stderr, "out of memory\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
    }
    Node *n = &t->nodes[t->count];
    *n = (Node) {center, half, 0.0, {0, 0, 0}, -1, {-1, -1, -1, -1, -1, -1, -1, -1}};
    return t->count++;
}

// Index (0-7) of the sub-cell of node n that contains position p.
static int octant(const Node *n, Vec3 p)
{
    return (p.x >= n->center.x) | (p.y >= n->center.y) << 1 | (p.z >= n->center.z) << 2;
}

static void insert(Tree *t, int n, const Vec3 *pos, int p, int depth)
{
    // Note: t->nodes may move during new_node(), so always index, never keep pointers.
    // Every cell on the path down accumulates the particle's mass and moment.
    t->nodes[n].mass += MASS;
    t->nodes[n].com.x += MASS * pos[p].x;
    t->nodes[n].com.y += MASS * pos[p].y;
    t->nodes[n].com.z += MASS * pos[p].z;

    if (t->nodes[n].child[0] == -1) {
        if (t->nodes[n].particle == -1) {
            t->nodes[n].particle = p;   // Empty leaf: store the particle here.
            return;
        }
        if (depth == MAX_DEPTH) {
            return;                     // Coincident particles: merge into this leaf.
        }
        // Occupied leaf: split it into 8 children and push the old particle down.
        for (int c = 0; c < 8; ++c) {
            const double h = t->nodes[n].half / 2;
            const Vec3 center = {
                t->nodes[n].center.x + (c & 1 ? h : -h),
                t->nodes[n].center.y + (c & 2 ? h : -h),
                t->nodes[n].center.z + (c & 4 ? h : -h),
            };
            const int child = new_node(t, center, h);
            t->nodes[n].child[c] = child;
        }
        const int old = t->nodes[n].particle;
        t->nodes[n].particle = -1;
        insert(t, t->nodes[n].child[octant(&t->nodes[n], pos[old])], pos, old, depth + 1);
    }
    // Inner node: continue in the sub-cell that contains the new particle.
    insert(t, t->nodes[n].child[octant(&t->nodes[n], pos[p])], pos, p, depth + 1);
}

static void build_tree(Tree *t, const Vec3 *pos, int n)
{
    // The root is a cube enclosing all particles.
    double lo = pos[0].x, hi = pos[0].x;
    for (int i = 0; i < n; ++i) {
        lo = fmin(lo, fmin(pos[i].x, fmin(pos[i].y, pos[i].z)));
        hi = fmax(hi, fmax(pos[i].x, fmax(pos[i].y, pos[i].z)));
    }
    const double mid = (lo + hi) / 2;
    t->count = 0;                                   // Reuse last timestep's memory.
    new_node(t, (Vec3) {mid, mid, mid}, (hi - lo) / 2 + 1e-9);

    for (int i = 0; i < n; ++i) {
        insert(t, 0, pos, i, 0);
    }
    // Turn the mass-weighted position sums into centers of mass.
    for (int k = 0; k < t->count; ++k) {
        if (t->nodes[k].mass > 0) {
            t->nodes[k].com.x /= t->nodes[k].mass;
            t->nodes[k].com.y /= t->nodes[k].mass;
            t->nodes[k].com.z /= t->nodes[k].mass;
        }
    }
}

// Adds the acceleration that a point mass m at q causes at p.
static void add_accel(Vec3 *a, Vec3 p, Vec3 q, double m)
{
    const double dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
    const double r2 = dx * dx + dy * dy + dz * dz + SOFTENING;
    const double s = G * m / (r2 * sqrt(r2));   // G m / r^3, times (dx,dy,dz) gives G m / r^2.
    a->x += s * dx;
    a->y += s * dy;
    a->z += s * dz;
}

// Barnes-Hut traversal: acceleration on particle i from the subtree of node n.
static void tree_accel(const Tree *t, int n, const Vec3 *pos, int i,
                       double theta2, Vec3 *a)
{
    const Node *node = &t->nodes[n];
    if (node->mass == 0 || node->particle == i) {
        return;                         // Empty cell, or the particle itself.
    }
    const Vec3 p = pos[i];
    const double dx = node->com.x - p.x, dy = node->com.y - p.y, dz = node->com.z - p.z;
    const double size = 2 * node->half;
    // Never approximate a cell that contains particle i itself.
    const int inside = fabs(p.x - node->center.x) <= node->half
                    && fabs(p.y - node->center.y) <= node->half
                    && fabs(p.z - node->center.z) <= node->half;
    // Opening criterion size / distance < theta, squared to avoid a sqrt.
    if (node->child[0] == -1
        || (!inside && size * size < theta2 * (dx * dx + dy * dy + dz * dz))) {
        add_accel(a, p, node->com, node->mass);   // Treat the cell as one point mass.
        return;
    }
    for (int c = 0; c < 8; ++c) {
        tree_accel(t, node->child[c], pos, i, theta2, a);   // Too close: open the cell.
    }
}

// Direct summation (theta == 0): acceleration on particle i from all others.
static void direct_accel(const Vec3 *pos, int n, int i, Vec3 *a)
{
    for (int j = 0; j < n; ++j) {
        if (j != i) {
            add_accel(a, pos[i], pos[j], MASS);
        }
    }
}

typedef enum {
    MODE_ALLGATHERV,
    MODE_PUT,
    MODE_COUNT
} Mode;

static const char *const MODE_NAMES[MODE_COUNT] = {"allgatherv", "put"};

// Returns the mode with the given name, or MODE_COUNT if there is none.
static Mode parse_mode(const char *name)
{
    Mode mode = MODE_ALLGATHERV;
    while (mode < MODE_COUNT && strcmp(name, MODE_NAMES[mode]) != 0) {
        ++mode;
    }
    return mode;
}

// Block distribution of all particles, in doubles (3 per position).
typedef struct {
    int rank, size;
    int *counts;   // Doubles owned by each rank.
    int *displs;   // Offset of each rank's first double.
} Layout;

static void exchange_allgatherv(double *pos, const Layout *l)
{
    MPI_Allgatherv(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
                   pos, l->counts, l->displs, MPI_DOUBLE, MPI_COMM_WORLD);
}

// Writes the own block into the window of every other rank with one MPI_Put per
// target. The window exposes pos, so a block has the same offset on the origin
// and on the target.
static void exchange_put(const double *pos, const Layout *l, MPI_Win win)
{
    const int offset = l->displs[l->rank];
    const int count = l->counts[l->rank];

    // No RMA calls precede this fence. It also makes the local position updates
    // visible in the window.
    MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
    for (int k = 1; k < l->size; ++k) {
        // Each rank starts with a different target to avoid hot spots.
        const int target = (l->rank + k) % l->size;
        MPI_Put(pos + offset, count, MPI_DOUBLE, target,
                offset, count, MPI_DOUBLE, win);
    }
    // Completes all transfers. There were no local stores in this epoch, and
    // nothing is put into any window until the next fence.
    MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOSUCCEED, win);
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const int n = argc > 1 ? atoi(argv[1]) : 5000;
    const int steps = argc > 2 ? atoi(argv[2]) : 100;
    const double theta = argc > 3 ? atof(argv[3]) : 0.5;
    const Mode mode = parse_mode(argc > 4 ? argv[4] : "put");
    const char *out_name = argc > 5 ? argv[5] : "data.dat";
    if (n < size || steps < 1 || theta < 0 || mode == MODE_COUNT) {
        if (rank == 0) {
            fprintf(stderr, "usage: %s [particles >= ranks] [timesteps] [theta] "
                    "[allgatherv|put] [output|-]\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    // Block distribution: rank r owns counts[r] particles starting at displs[r].
    // Counts are in doubles (3 per position) so MPI_DOUBLE can be used directly.
    int *counts = malloc((size_t) size * sizeof(int));
    int *displs = malloc((size_t) size * sizeof(int));
    for (int r = 0; r < size; ++r) {
        const int first = (int) ((long) n * r / size);
        const int last = (int) ((long) n * (r + 1) / size);
        counts[r] = 3 * (last - first);
        displs[r] = 3 * first;
    }
    const int first = displs[rank] / 3;
    const int local_n = counts[rank] / 3;

    const Layout layout = {rank, size, counts, displs};

    // The positions live in memory allocated by MPI and exposed as a window, so
    // that other ranks can write into it. All modes use the same allocation.
    double *window_memory;
    MPI_Win win;
    MPI_Win_allocate((MPI_Aint) n * (MPI_Aint) sizeof(Vec3), sizeof(double),
                     MPI_INFO_NULL, MPI_COMM_WORLD, &window_memory, &win);

    // Every rank generates the same initial state (same seed, same order as
    // Assignment 5), so no broadcast is needed. Velocities start at zero.
    Vec3 *pos = (Vec3 *) window_memory;
    Vec3 *vel = calloc((size_t) local_n, sizeof(Vec3));   // Only own particles.
    srand(42);
    for (int i = 0; i < n; ++i) {
        pos[i].x = RANGE * rand() / RAND_MAX;
        pos[i].y = RANGE * rand() / RAND_MAX;
        pos[i].z = RANGE * rand() / RAND_MAX;
    }

    // Rank 0 keeps all frames in memory; they are written after timing.
    const int write = rank == 0 && strcmp(out_name, "-") != 0;
    Vec3 *traj = write ? malloc((size_t) n * steps * sizeof(Vec3)) : NULL;
    if (vel == NULL || (write && traj == NULL)) {
        fprintf(stderr, "out of memory\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    Tree tree = {0};
    double t_build = 0, t_force = 0, t_comm = 0;   // Per-phase times of this rank.
    MPI_Barrier(MPI_COMM_WORLD);                  // Start all ranks together.
    const double start = MPI_Wtime();

    for (int step = 0; step < steps; ++step) {
        double t0 = MPI_Wtime();
        if (theta > 0) {
            build_tree(&tree, pos, n);            // Replicated on every rank.
        }
        double t1 = MPI_Wtime();

        // Update velocities first: all forces must use the old positions.
        for (int k = 0; k < local_n; ++k) {
            Vec3 a = {0, 0, 0};
            if (theta > 0) {
                tree_accel(&tree, 0, pos, first + k, theta * theta, &a);
            } else {
                direct_accel(pos, n, first + k, &a);
            }
            vel[k].x += a.x;                      // dt = 1
            vel[k].y += a.y;
            vel[k].z += a.z;
        }
        // Then move the own particles in place.
        for (int k = 0; k < local_n; ++k) {
            pos[first + k].x += vel[k].x;
            pos[first + k].y += vel[k].y;
            pos[first + k].z += vel[k].z;
        }
        double t2 = MPI_Wtime();

        // Share the new positions: afterwards every rank has all of them again.
        if (mode == MODE_PUT) {
            exchange_put(window_memory, &layout, win);
        } else {
            exchange_allgatherv(window_memory, &layout);
        }
        double t3 = MPI_Wtime();

        t_build += t1 - t0;
        t_force += t2 - t1;
        t_comm += t3 - t2;                        // Includes waiting for slower ranks.
        if (write) {
            memcpy(traj + (size_t) step * n, pos, (size_t) n * sizeof(Vec3));
        }
    }

    // The slowest rank determines the wall time; also report per-phase maxima.
    double local[4] = {MPI_Wtime() - start, t_build, t_force, t_comm}, global[4];
    MPI_Reduce(local, global, 4, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (write) {
        FILE *f = fopen(out_name, "w");
        if (f == NULL) {
            perror(out_name);
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        for (int step = 0; step < steps; ++step) {
            for (int i = 0; i < n; ++i) {
                const Vec3 p = traj[(size_t) step * n + i];
                fprintf(f, "%.10f %.10f %.10f\n", p.x, p.y, p.z);
            }
            fputs("\n\n", f);   // Two blank lines end a gnuplot frame.
        }
        fclose(f);
    }
    if (rank == 0) {
        // One key=value line, easy to read and easy to turn into CSV.
        printf("mode=%s ranks=%d particles=%d timesteps=%d theta=%g time=%.6f "
               "build=%.6f force=%.6f comm=%.6f\n",
               MODE_NAMES[mode], size, n, steps, theta,
               global[0], global[1], global[2], global[3]);
    }

    free(traj);
    free(tree.nodes);
    free(vel);
    free(displs);
    free(counts);
    MPI_Win_free(&win);   // Also releases the positions.
    MPI_Finalize();
    return EXIT_SUCCESS;
}
