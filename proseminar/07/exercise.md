# Assignment 07

## Exercise 1

### Which one-sided method did you choose and why?

`MPI_Put` with `MPI_Win_fence`. It replaces the `MPI_Allgatherv` that shared the new positions after every timestep.

- All ranks exchange data in every step with sizes and offsets known in advance, and all positions must have arrived before the next force computation. A collective fence matches this bulk-synchronous pattern.
- Each rank writes a different part of the position array, so there are no conflicting writes.
- With `Put`, the owner of a block pushes it as soon as it is updated.

### How is the one-sided communication done?

1. **Window:** `MPI_Win_allocate` allocates the position array of all `N` particles on every rank and makes it accessible to the other ranks. The displacement unit is one `double`.
2. **Local update:** each rank moves its own block of particles.
3. **Open the epoch:** the first `MPI_Win_fence` starts the phase in which remote accesses are allowed.
4. **Transfer:** each rank writes its block into the window of every other rank. Since all windows have the same layout, the offset is the same locally and remotely. The targets are not involved and post no receive.
5. **Close the epoch:** the second `MPI_Win_fence` completes all transfers. Only now may the received positions be read and the own block be changed again.

```c
MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
for (int k = 1; k < size; ++k) {
    const int target = (rank + k) % size;   // staggered to avoid hot spots
    MPI_Put(pos + offset, count, MPI_DOUBLE,           // own block (origin)
            target, offset, count, MPI_DOUBLE, win);   // same place on target
}
MPI_Win_fence(MPI_MODE_NOSTORE | MPI_MODE_NOPUT | MPI_MODE_NOSUCCEED, win);
```

The assertions allow MPI to optimize:

- no transfers precede the first fence (`NOPRECEDE`)
- or follow the second (`NOSUCCEED`)
- the window was not written locally during the epoch (`NOSTORE`)
- and nothing is put into it before the next fence (`NOPUT`).

### What are the trade-offs with respect to the other one-sided methods?

| method | advantage | disadvantage here |
|---|---|---|
| fence (chosen) | simple, one clear completion point | all ranks synchronize and wait for the slowest |
| post/start/complete/wait | synchronizes only named groups | every group would contain all other ranks: more code, little gain |
| lock/unlock | targets do not take part | targets cannot tell when data has arrived: needs flush plus barrier or notification |
| shared memory window | no copies within a node | only works within a node, a second exchange between nodes is still needed |

### Speedup and efficiency compared to two-sided communication

- 1–96 ranks
- `N` = 1000–10000
- `θ` = 0, 0.5, 1, 100 timesteps
- fastest of 3 runs
- relative to 1 rank

(`sbatch benchmark.slurm`, then `gnuplot -e "data='nbody_rma_<jobid>.csv'" plot_benchmark.plt`).

![Speedup and efficiency](scaling.svg)

![Total and communication time](comm.svg)

_TODO: observations from the LCC3 results._
