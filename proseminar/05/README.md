# Assignment 5

The goal of this assignment is to implement a 3D n-body simulation.

## Exercise 1 (2 Points)

### Description

N-body simulations are one of the 7 original dwarfs of HPC and are used in research ranging from astrophysics to molecular dynamics. At their core, they model and simulate the interaction of moving particles in physical space. For this assignment, the specific n-body setting relates to astrophysics, where the mutual gravitational effect of stars is investigated. Each particle has several properties which include at least

- position,
- velocity, and
- mass.

For each timestep (you can assume `dt = 1`), particles must be moved by first computing the force exerted on them according to the [Newtonian equation for gravity](https://en.wikipedia.org/wiki/Newton%27s_law_of_universal_gravitation), `force = G * (mass_1 * mass_2) / radius^2` where `G` is the gravitational constant (and can be assumed as `G = 1` for simplicity). Second, using the computed force on a particle, its position and velocity can be updated via `velocity = velocity + force / mass` and `position = position + velocity`.

### Tasks

- Provide a sequential implementation of the n-body simulation in 3D. While making any implementation design decisions, consider that this application will have to be parallelized using MPI in a future assignment. Hints on how to proceed (not mandatory to follow):
    1. generate particles randomly, e.g. uniformly distributed
    2. provide a function for computing forces and moving particles
    3. move particles in a time loop for a given number of steps
- Measure the execution time for various particle numbers and timesteps. What can you observe?
- Write your data to a file (e.g. `data.dat` ) and use any visualization tool to visualize the movement of the data.
  - [Here is an example](particle.plt) using `gnuplot`. You can adapt this file according to your particle range implementation and any other needs.
  - The data for this example visualization has to be in following format, showing x, y, and z coordinates per particle on one line:

    ````text
        x1_timestep1 y1_timestep1 z1_timestep1
        x2_timestep1 y2_timestep1 z2_timestep1
        x3_timestep1 y3_timestep1 z3_timestep1
    
    
        x1_timestep2 y1_timestep2 z1_timestep2
        x2_timestep2 y2_timestep2 z2_timestep2
        x3_timestep2 y3_timestep2 z3_timestep2
    
    
        x1_timestep3 y1_timestep3 z1_timestep3
        x2_timestep3 y2_timestep3 z2_timestep3
        x3_timestep3 y3_timestep3 z3_timestep3
    ````

       Having 2 blank lines means the points for this timestep are finished. Note that all values can be floating point numbers.
- For measuring performance either disable writing to a datafile or consider not dumping each timestep. Measure only the actual n-body simulation, not any initialization or data I/O.
- Benchmark the sequential program using 5000 particles and 100 time steps and enter the results into the comparison spreadsheet: https://docs.google.com/spreadsheets/d/1Tj2Ic-kQIPCrq0ZIpKfPUeFV1pf2I1sT1qbMEEmIydw

## Exercise 2 (1 Point)

This exercise consists in investigating and planning optimization and parallelization techniques for Exercise 1.

### Tasks

- Study the nature of the problem in Exercise 1, focusing on its characteristics with regard to optimization and parallelization with MPI.
- What optimization methods can you come up with in order to improve the performance of Exercise 1?
- What parallelization strategies would you consider for Exercise 1 and why?

### Analysis

The simulation is an all-pairs, direct-summation N-body problem. For every
timestep, the force on each of the `N` particles is computed from all other
particles, so the force calculation performs `O(N^2)` interactions and the
complete simulation performs `O(TN^2)` work for `T` timesteps. The state and
force arrays require `O(N)` memory. The force on each target particle can be
computed independently within one timestep, but the next timestep cannot start
until all forces and particle updates from the current timestep are complete.
Because gravity has unlimited range, every particle potentially depends on
every other particle; this makes communication, rather than only local
computation, important in an MPI implementation.

### Optimization methods

- Exploit Newton's third law: calculate each pair `(i, j)` only once and add
  the equal-and-opposite force to both particles. This nearly halves the
  interaction work, although the updates must be arranged carefully to avoid
  data races in a threaded version.
- Use the vector form
  `force_vector = G * m_i * m_j * (r_j - r_i) / |r_j - r_i|^3`. This needs
  one square-root operation per pair rather than separately normalizing each
  component, and a small softening parameter can prevent singularities when
  particles are extremely close.
- Improve cache and SIMD behavior by storing coordinates, velocities, and
  masses in a structure-of-arrays layout, keeping inner-loop values in local
  variables, and compiling with the appropriate optimization and architecture
  flags. The inner interaction loop is a good candidate for compiler
  vectorization.
- Avoid unnecessary output during benchmarking. Writing every trajectory frame
  is much slower than the simulation and should be disabled or sampled only at
  selected timesteps.
- For larger simulations, replace direct summation with an approximate
  Barnes--Hut or fast multipole method. These reduce the computational
  complexity (approximately `O(N log N)` for Barnes--Hut), at the cost of an
  accuracy parameter and a more complicated tree.

### MPI parallelization strategies

The simplest strategy is a one-dimensional decomposition by target particle.
Each rank owns a contiguous subset of particles and computes the forces for
those targets. At every timestep, ranks need the positions (and masses) of all
particles, so the global state can be exchanged with `MPI_Allgatherv`; each
rank then updates only its local particles. A second `MPI_Allgatherv` (or an
equivalent exchange) makes the updated state available for the next timestep.
This approach is straightforward, balances well when particles have equal
cost, and reduces computation per rank from `O(N^2)` to approximately
`O(N^2/P)`, but it replicates the particle state and communicates `O(N)` data
per timestep.

For a more communication-efficient direct solver, decompose the *pair
interactions* into blocks. Each rank computes a subset of `(i, j)` pairs,
accumulates partial forces, and combines them with `MPI_Allreduce`. Pair
symmetry can be used so only one triangle of the interaction matrix is
processed. This reduces redundant force calculations, but the reduction and
the need to distribute particle data make the implementation more complex.

Spatial/domain decomposition is less effective for the exact algorithm because
long-range gravity gives every particle a non-local dependency. It becomes
attractive only with a cutoff/approximation, such as a Barnes--Hut tree, where
each rank can exchange boundary or summarized tree data instead of all
particles. In all designs, initialization and file output should be performed
by one rank (or gathered only when necessary), and a barrier or collective
operation must preserve timestep ordering. Scaling should be evaluated by
measuring runtime, speedup, parallel efficiency, and communication time for
several values of `N` and `P`.
