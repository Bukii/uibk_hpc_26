#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

typedef double value_t;
typedef value_t *Vector;

static Vector createVector(int rows, int columns) {
  return malloc(sizeof(value_t) * (size_t)rows * (size_t)columns);
}

static void releaseVector(Vector m) { free(m); }

static void setBoundaryHalos(Vector m, int rows, int columns, int up, int down,
                             int left, int right) {
  int stride = columns + 2;
  if (up == MPI_PROC_NULL)
    for (int x = 1; x <= columns; x++)
      m[x] = m[stride + x];
  if (down == MPI_PROC_NULL)
    for (int x = 1; x <= columns; x++)
      m[(rows + 1) * stride + x] = m[rows * stride + x];
  if (left == MPI_PROC_NULL)
    for (int y = 1; y <= rows; y++)
      m[y * stride] = m[y * stride + 1];
  if (right == MPI_PROC_NULL)
    for (int y = 1; y <= rows; y++)
      m[y * stride + columns + 1] = m[y * stride + columns];
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int N = 2000;
  if (argc > 1)
    N = atoi(argv[1]);
  int T = N * 50;
  if (argc > 2)
    T = atoi(argv[2]);
  if (N <= 0 || T < 0) {
    if (rank == 0)
      fprintf(stderr, "Require N > 0 and T >= 0.\n");
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  // CHANGED FOR MPI: arrange the processes and their subdomains in 2D.
  int dimensions[2] = {0, 0};
  MPI_Dims_create(size, 2, dimensions);
  if (dimensions[0] > N || dimensions[1] > N) {
    if (rank == 0)
      fprintf(stderr, "The process grid must not exceed the grid size in either dimension.\n");
    MPI_Finalize();
    return EXIT_FAILURE;
  }
  int periods[2] = {0, 0};
  MPI_Comm cartesian;
  MPI_Cart_create(MPI_COMM_WORLD, 2, dimensions, periods, 0, &cartesian);
  int coordinates[2];
  MPI_Cart_coords(cartesian, rank, 2, coordinates);
  int up, down, left, right;
  MPI_Cart_shift(cartesian, 0, 1, &up, &down);
  MPI_Cart_shift(cartesian, 1, 1, &left, &right);

  int rows = N / dimensions[0] + (coordinates[0] < N % dimensions[0]);
  int columns = N / dimensions[1] + (coordinates[1] < N % dimensions[1]);
  int row_offset = coordinates[0] * (N / dimensions[0]) +
                   (coordinates[0] < N % dimensions[0] ? coordinates[0]
                                                        : N % dimensions[0]);
  int column_offset = coordinates[1] * (N / dimensions[1]) +
                      (coordinates[1] < N % dimensions[1] ? coordinates[1]
                                                           : N % dimensions[1]);
  int stride = columns + 2;
  Vector A = createVector(rows + 2, columns + 2);
  Vector B = createVector(rows + 2, columns + 2);
  if (A == NULL || B == NULL)
    MPI_Abort(cartesian, EXIT_FAILURE);
  for (int i = 0; i < (rows + 2) * (columns + 2); i++)
    A[i] = B[i] = 273;

  int source_x = N / 4, source_y = N / 4;
  int source_local_x = source_x - column_offset + 1;
  int source_local_y = source_y - row_offset + 1;
  int owns_source = source_local_x >= 1 && source_local_x <= columns &&
                    source_local_y >= 1 && source_local_y <= rows;
  if (owns_source)
    A[source_local_y * stride + source_local_x] = 333;

  MPI_Datatype column_type;
  MPI_Type_vector(rows, 1, stride, MPI_DOUBLE, &column_type);
  MPI_Type_commit(&column_type);
  if (rank == 0)
    printf("Computing 2D heat-distribution for N=%dx%d, T=%d using %d MPI processes\n",
           N, N, T, size);

  MPI_Barrier(cartesian);
  double start = MPI_Wtime();
  for (int t = 0; t < T; t++) {
    setBoundaryHalos(A, rows, columns, up, down, left, right);
    // CHANGED FOR MPI: post all four non-blocking halo exchanges first.
    MPI_Request requests[8];
    MPI_Irecv(&A[1], columns, MPI_DOUBLE, up, 0, cartesian, &requests[0]);
    MPI_Irecv(&A[(rows + 1) * stride + 1], columns, MPI_DOUBLE, down, 1,
              cartesian, &requests[1]);
    MPI_Irecv(&A[stride], 1, column_type, left, 2, cartesian, &requests[2]);
    MPI_Irecv(&A[stride + columns + 1], 1, column_type, right, 3, cartesian,
              &requests[3]);
    MPI_Isend(&A[stride + 1], columns, MPI_DOUBLE, up, 1, cartesian,
              &requests[4]);
    MPI_Isend(&A[rows * stride + 1], columns, MPI_DOUBLE, down, 0, cartesian,
              &requests[5]);
    MPI_Isend(&A[stride + 1], 1, column_type, left, 3, cartesian,
              &requests[6]);
    MPI_Isend(&A[stride + columns], 1, column_type, right, 2, cartesian,
              &requests[7]);

    // CHANGED FOR MPI: compute cells whose four neighbors are local.
    for (int y = 2; y < rows; y++)
      for (int x = 2; x < columns; x++) {
        int i = y * stride + x;
        if (owns_source && x == source_local_x && y == source_local_y)
          B[i] = A[i];
        else
          B[i] = A[i] + 0.1 * (A[i - 1] + A[i + 1] + A[i - stride] +
                               A[i + stride] - 4 * A[i]);
      }

    // CHANGED FOR MPI: wait only after useful inner-cell work is complete.
    MPI_Waitall(8, requests, MPI_STATUSES_IGNORE);
    for (int y = 1; y <= rows; y++)
      for (int x = 1; x <= columns; x++) {
        if (y != 1 && y != rows && x != 1 && x != columns)
          continue;
        int i = y * stride + x;
        if (owns_source && x == source_local_x && y == source_local_y)
          B[i] = A[i];
        else
          B[i] = A[i] + 0.1 * (A[i - 1] + A[i + 1] + A[i - stride] +
                               A[i + stride] - 4 * A[i]);
      }
    Vector H = A;
    A = B;
    B = H;
  }

  double local_time = MPI_Wtime() - start, elapsed;
  MPI_Reduce(&local_time, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, cartesian);
  int local_success = 1;
  for (int y = 1; y <= rows; y++)
    for (int x = 1; x <= columns; x++)
      if (A[y * stride + x] < 273 || A[y * stride + x] > 333)
        local_success = 0;
  int success;
  MPI_Reduce(&local_success, &success, 1, MPI_INT, MPI_MIN, 0, cartesian);
  if (rank == 0) {
    printf("Wall time: %.6f seconds\n", elapsed);
    printf("Verification: %s\n", success ? "OK" : "FAILED");
  }
  MPI_Type_free(&column_type);
  releaseVector(A);
  releaseVector(B);
  MPI_Comm_free(&cartesian);
  MPI_Finalize();
  return (rank == 0 && !success) ? EXIT_FAILURE : EXIT_SUCCESS;
}
