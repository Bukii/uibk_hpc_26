#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

typedef double value_t;

typedef value_t *Vector;

static Vector createVector(int rows, int columns);
static void releaseVector(Vector m);
static void exchangeHalos(Vector m, int rows, int columns, MPI_Comm comm,
                          int up, int down, int left, int right,
                          MPI_Datatype column_type);
static void setBoundaryHalos(Vector m, int rows, int columns, int up, int down,
                              int left, int right);

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);

  int rank;
  int size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  int N = 2000;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  int T = N * 50;
  if (argc > 2) {
    T = atoi(argv[2]);
  }

  int valid_input = N > 0 && T >= 0;
  if (!valid_input) {
    if (rank == 0) {
      fprintf(stderr, "N must be positive and T must not be negative.\n");
    }
    MPI_Finalize();
    return EXIT_FAILURE;
  }

  // CHANGED FOR MPI: arrange processes in a two-dimensional Cartesian grid.
  int dimensions[2] = {0, 0};
  MPI_Dims_create(size, 2, dimensions);
  int periods[2] = {0, 0};
  MPI_Comm cartesian;
  MPI_Cart_create(MPI_COMM_WORLD, 2, dimensions, periods, 0, &cartesian);

  int coordinates[2];
  MPI_Cart_coords(cartesian, rank, 2, coordinates);
  int up, down, left, right;
  MPI_Cart_shift(cartesian, 0, 1, &up, &down);
  MPI_Cart_shift(cartesian, 1, 1, &left, &right);

  // CHANGED FOR MPI: distribute rows and columns, including uneven remainders.
  int rows = N / dimensions[0] + (coordinates[0] < N % dimensions[0]);
  int columns = N / dimensions[1] + (coordinates[1] < N % dimensions[1]);
  int row_offset = coordinates[0] * (N / dimensions[0]) +
                   (coordinates[0] < N % dimensions[0]
                        ? coordinates[0]
                        : N % dimensions[0]);
  int column_offset = coordinates[1] * (N / dimensions[1]) +
                      (coordinates[1] < N % dimensions[1]
                           ? coordinates[1]
                           : N % dimensions[1]);

  // One-cell halo on every side; the interior starts at (1, 1).
  Vector A = createVector(rows + 2, columns + 2);
  Vector B = createVector(rows + 2, columns + 2);
  if (A == NULL || B == NULL) {
    fprintf(stderr, "Rank %d could not allocate its temperature fields.\n", rank);
    releaseVector(A);
    releaseVector(B);
    MPI_Abort(cartesian, EXIT_FAILURE);
  }

  for (int y = 0; y < rows + 2; y++) {
    for (int x = 0; x < columns + 2; x++) {
      A[y * (columns + 2) + x] = 273;
      B[y * (columns + 2) + x] = 273;
    }
  }

  int source_x = N / 4;
  int source_y = N / 4;
  int source_local_x = source_x - column_offset + 1;
  int source_local_y = source_y - row_offset + 1;
  int owns_source = source_local_x >= 1 && source_local_x <= columns &&
                    source_local_y >= 1 && source_local_y <= rows;
  if (owns_source) {
    A[source_local_y * (columns + 2) + source_local_x] = 273 + 60;
  }

  MPI_Datatype column_type;
  MPI_Type_vector(rows, 1, columns + 2, MPI_DOUBLE, &column_type);
  MPI_Type_commit(&column_type);

  if (rank == 0) {
    printf("Computing heat-distribution for room size N=%dx%d for T=%d "
           "timesteps using %d MPI processes\n",
           N, N, T, size);
  }

  MPI_Barrier(cartesian);
  double start = MPI_Wtime();

  for (int t = 0; t < T; t++) {
    setBoundaryHalos(A, rows, columns, up, down, left, right);
    exchangeHalos(A, rows, columns, cartesian, up, down, left, right,
                  column_type);

    for (int y = 1; y <= rows; y++) {
      for (int x = 1; x <= columns; x++) {
        int i = y * (columns + 2) + x;
        if (owns_source && x == source_local_x && y == source_local_y) {
          B[i] = A[i];
          continue;
        }

        value_t tc = A[i];
        value_t tl = A[i - 1];
        value_t tr = A[i + 1];
        value_t tu = A[i - (columns + 2)];
        value_t td = A[i + (columns + 2)];
        B[i] = tc + 0.1 * (tl + tr + tu + td + (-4 * tc));
      }
    }

    Vector H = A;
    A = B;
    B = H;
  }

  double local_time = MPI_Wtime() - start;
  double elapsed;
  MPI_Reduce(&local_time, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, cartesian);

  int local_success = 1;
  for (int y = 1; y <= rows && local_success; y++) {
    for (int x = 1; x <= columns; x++) {
      value_t temp = A[y * (columns + 2) + x];
      if (temp < 273 || temp > 273 + 60) {
        local_success = 0;
        break;
      }
    }
  }
  int success;
  MPI_Reduce(&local_success, &success, 1, MPI_INT, MPI_MIN, 0, cartesian);

  if (rank == 0) {
    printf("Wall time: %.6f seconds\n", elapsed);
    printf("Verification: %s\n", success ? "OK" : "FAILED");
  }

  MPI_Type_free(&column_type);
  releaseVector(B);
  releaseVector(A);
  MPI_Comm_free(&cartesian);
  MPI_Finalize();
  return (rank == 0 && !success) ? EXIT_FAILURE : EXIT_SUCCESS;
}

static Vector createVector(int rows, int columns) {
  return malloc(sizeof(value_t) * (size_t)rows * (size_t)columns);
}

static void releaseVector(Vector m) { free(m); }

static void setBoundaryHalos(Vector m, int rows, int columns, int up, int down,
                             int left, int right) {
  int stride = columns + 2;
  if (up == MPI_PROC_NULL) {
    for (int x = 1; x <= columns; x++) {
      m[x] = m[stride + x];
    }
  }
  if (down == MPI_PROC_NULL) {
    for (int x = 1; x <= columns; x++) {
      m[(rows + 1) * stride + x] = m[rows * stride + x];
    }
  }
  if (left == MPI_PROC_NULL) {
    for (int y = 1; y <= rows; y++) {
      m[y * stride] = m[y * stride + 1];
    }
  }
  if (right == MPI_PROC_NULL) {
    for (int y = 1; y <= rows; y++) {
      m[y * stride + columns + 1] = m[y * stride + columns];
    }
  }
}

static void exchangeHalos(Vector m, int rows, int columns, MPI_Comm comm,
                          int up, int down, int left, int right,
                          MPI_Datatype column_type) {
  int stride = columns + 2;

  MPI_Sendrecv(&m[stride + 1], columns, MPI_DOUBLE, up, 0,
               &m[(rows + 1) * stride + 1], columns, MPI_DOUBLE, down, 0, comm,
               MPI_STATUS_IGNORE);
  MPI_Sendrecv(&m[rows * stride + 1], columns, MPI_DOUBLE, down, 1,
               &m[1], columns, MPI_DOUBLE, up, 1, comm, MPI_STATUS_IGNORE);
  MPI_Sendrecv(&m[stride + 1], 1, column_type, left, 2,
               &m[stride + columns + 1], 1, column_type, right, 2, comm,
               MPI_STATUS_IGNORE);
  MPI_Sendrecv(&m[stride + columns], 1, column_type, right, 3,
               &m[stride], 1, column_type, left, 3, comm, MPI_STATUS_IGNORE);
}
