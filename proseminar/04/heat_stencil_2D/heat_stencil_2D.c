#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef double value_t;

#define RESOLUTION 120

// -- vector utilities --

typedef value_t *Vector;

// CHANGED FOR 2D: the vector stores an N x N temperature field.
Vector createVector(int N);

void releaseVector(Vector m);

// CHANGED FOR 2D: print the two-dimensional temperature field.
void printTemperature(Vector m, int N);

// -- simulation code ---

int main(int argc, char **argv) {
  // 'parsing' optional input parameter = problem size
  int N = 2000;
  if (argc > 1) {
    N = atoi(argv[1]);
  }
  // CHANGED FOR 2D: the optional second parameter sets the timesteps.
  int T = N * 50;
  if (argc > 2) {
    T = atoi(argv[2]);
  }
  printf("Computing heat-distribution for room size N=%dx%d for T=%d timesteps\n", N, N, T);

  // ---------- setup ----------

  // create a buffer for storing temperature fields
  Vector A = createVector(N);

  // set up initial conditions in A
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      A[y * N + x] = 273; // temperature is 0° C everywhere (273 K)
    }
  }

  // and there is a heat source in one corner
  // CHANGED FOR 2D: address the source by its x and y coordinates.
  int source_x = N / 4;
  int source_y = N / 4;
  A[source_y * N + source_x] = 273 + 60;

  printf("Initial:\n");
  printTemperature(A, N);
  printf("\n");

  // ---------- compute ----------

  // create a second buffer for the computation
  Vector B = createVector(N);

  struct timespec start;
  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    perror("clock_gettime");
    releaseVector(B);
    releaseVector(A);
    return EXIT_FAILURE;
  }

  // for each time step ..
  for (int t = 0; t < T; t++) {
    // .. we propagate the temperature
    // CHANGED FOR 2D: update every cell in the two-dimensional field.
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        int i = y * N + x;

        // center stays constant (the heat is still on)
        if (x == source_x && y == source_y) {
          B[i] = A[i];
          continue;
        }

        // get temperature at current position
        value_t tc = A[i];

        // get temperatures of adjacent cells
        // CHANGED FOR 2D: use the four neighbors of the current cell.
        value_t tl = (x != 0) ? A[i - 1] : tc;
        value_t tr = (x != N - 1) ? A[i + 1] : tc;
        value_t tu = (y != 0) ? A[i - N] : tc;
        value_t td = (y != N - 1) ? A[i + N] : tc;

        // compute new temperature at current position
        B[i] = tc + 0.1 * (tl + tr + tu + td + (-4 * tc));
      }
    }

    // swap matrices (just pointers, not content)
    Vector H = A;
    A = B;
    B = H;

    // show intermediate step
    // if (!(t % 1000)) {
    //   printf("Step t=%d:\n", t);
    //   printTemperature(A, N);
    //   printf("\n");
    // }
  }

  struct timespec end;
  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
    perror("clock_gettime");
    releaseVector(B);
    releaseVector(A);
    return EXIT_FAILURE;
  }
  double wall_time = (double)(end.tv_sec - start.tv_sec) +
                     (double)(end.tv_nsec - start.tv_nsec) / 1e9;
  printf("Wall time: %.6f seconds\n", wall_time);

  releaseVector(B);

  // ---------- check ----------

  printf("Final:\n");
  printTemperature(A, N);
  printf("\n");

  int success = 1;
  for (long long i = 0; i < (long long)N * N; i++) {
    value_t temp = A[i];
    if (273 <= temp && temp <= 273 + 60)
      continue;
    success = 0;
    break;
  }

  printf("Verification: %s\n", (success) ? "OK" : "FAILED");

  // ---------- cleanup ----------

  releaseVector(A);

  // done
  return (success) ? EXIT_SUCCESS : EXIT_FAILURE;
}

Vector createVector(int N) {
  // create data and index vector
  // CHANGED FOR 2D: allocate space for N rows of N values.
  return malloc(sizeof(value_t) * N * N);
}

void releaseVector(Vector m) { free(m); }

void printTemperature(Vector m, int N) {
  const char *colors = " .-:=+*^X#%@";
  const int numColors = 12;

  // boundaries for temperature (for simplicity hard-coded)
  const value_t max = 273 + 30;
  const value_t min = 273 + 0;

  // set the 'render' resolution
  int W = (N < RESOLUTION) ? N : RESOLUTION;

  // step size in each dimension
  // CHANGED FOR 2D: render square tiles in both dimensions.

  // room
  for (int j = 0; j < W; j++) {
    // actual room
    for (int i = 0; i < W; i++) {
      int x_start = N * i / W;
      int x_end = N * (i + 1) / W;
      int y_start = N * j / W;
      int y_end = N * (j + 1) / W;

      // get max temperature in this tile
      value_t max_t = 0;
      for (int y = y_start; y < y_end; y++) {
        for (int x = x_start; x < x_end; x++) {
          max_t = (max_t < m[y * N + x]) ? m[y * N + x] : max_t;
        }
      }
      value_t temp = max_t;

      // pick the 'color'
      int c = ((temp - min) / (max - min)) * numColors;
      c = (c >= numColors) ? numColors - 1 : ((c < 0) ? 0 : c);

      // print the average temperature
      printf("%c", colors[c]);
    }
    printf("\n");
  }
}
