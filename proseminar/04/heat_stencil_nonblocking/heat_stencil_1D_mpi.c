#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

typedef double value_t;
typedef value_t *Vector;

static Vector createVector(int N) {
  return malloc(sizeof(value_t) * (size_t)N);
}

static void releaseVector(Vector m) { free(m); }

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int N = 2000;
  if (argc > 1)
    N = atoi(argv[1]);
  int T = N * 500;
  if (argc > 2)
    T = atoi(argv[2]);

  if (N <= 0 || T < 0 || size > N) {
    if (rank == 0)
      fprintf(stderr, "Require N > 0, T >= 0, and at most N MPI processes.\n");
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  int base = N / size;
  int local_n = base + (rank < N % size);
  int offset = rank * base + (rank < N % size ? rank : N % size);
  int left = rank == 0 ? MPI_PROC_NULL : rank - 1;
  int right = rank == size - 1 ? MPI_PROC_NULL : rank + 1;

  // CHANGED FOR MPI: two ghost cells keep the sequential indexing readable.
  Vector A = createVector(local_n + 2);
  Vector B = createVector(local_n + 2);
  if (A == NULL || B == NULL) {
    fprintf(stderr, "Rank %d could not allocate its temperature fields.\n", rank);
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
  }
  for (int i = 0; i < local_n + 2; i++)
    A[i] = B[i] = 273;

  int source = N / 4;
  if (source >= offset && source < offset + local_n)
    A[source - offset + 1] = 273 + 60;

  if (rank == 0)
    printf("Computing 1D heat-distribution for N=%d, T=%d using %d MPI processes\n",
           N, T, size);

  MPI_Barrier(MPI_COMM_WORLD);
  double start = MPI_Wtime();
  for (int t = 0; t < T; t++) {
    // CHANGED FOR MPI: start communication before computing the inner cells.
    MPI_Request requests[4];
    MPI_Irecv(&A[0], 1, MPI_DOUBLE, left, 0, MPI_COMM_WORLD, &requests[0]);
    MPI_Irecv(&A[local_n + 1], 1, MPI_DOUBLE, right, 1, MPI_COMM_WORLD,
              &requests[1]);
    MPI_Isend(&A[1], 1, MPI_DOUBLE, left, 1, MPI_COMM_WORLD, &requests[2]);
    MPI_Isend(&A[local_n], 1, MPI_DOUBLE, right, 0, MPI_COMM_WORLD,
              &requests[3]);

    // CHANGED FOR MPI: cells away from the subdomain boundaries need no halo.
    for (int i = 2; i < local_n; i++) {
      if (offset + i - 1 == source)
        B[i] = A[i];
      else
        B[i] = A[i] + 0.2 * (A[i - 1] + A[i + 1] - 2 * A[i]);
    }

    // CHANGED FOR MPI: finish the ghost exchange before boundary cells.
    MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);
    for (int i = 1; i <= local_n; i++) {
      if (i != 1 && i != local_n)
        continue;
      if (offset + i - 1 == source)
        B[i] = A[i];
      else
        B[i] = A[i] + 0.2 * (A[i - 1] + A[i + 1] - 2 * A[i]);
    }

    Vector H = A;
    A = B;
    B = H;
  }
  double local_time = MPI_Wtime() - start;
  double elapsed;
  MPI_Reduce(&local_time, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0,
             MPI_COMM_WORLD);

  int local_success = 1;
  for (int i = 1; i <= local_n; i++)
    if (A[i] < 273 || A[i] > 333)
      local_success = 0;
  int success;
  MPI_Reduce(&local_success, &success, 1, MPI_INT, MPI_MIN, 0,
             MPI_COMM_WORLD);
  if (rank == 0) {
    printf("Wall time: %.6f seconds\n", elapsed);
    printf("Verification: %s\n", success ? "OK" : "FAILED");
  }

  releaseVector(A);
  releaseVector(B);
  MPI_Finalize();
  return (rank == 0 && !success) ? EXIT_FAILURE : EXIT_SUCCESS;
}
