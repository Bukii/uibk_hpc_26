# MPI Example Debug Report

## Scope

The examples in this directory were compiled and executed with Open MPI. No
files outside this directory were intentionally changed.

## Issues found and fixes attempted

### `example_1.c`

- **Mismatched message tags:** rank 0 sent with tag `42`, while every
  non-root rank received with tag `43`. The receive could not match the send,
  so the program hung at the communication step.
- **Unused variable:** the function-scope variable `i` was never used and was
  reported by `-Wall -Wextra`.
- **Array argument clarity:** `&data` was changed to `data`. Both expressions
  refer to the same storage for this MPI call, but `data` expresses the
  intended pointer to the first element.

The receive now uses the same `tag` as the send, and the unused `tag2` and
`i` declarations were removed.

### `example_2.c`

- **Rank was overwritten:** expressions such as
  `(rank = 1 + size) % size` assigned to `rank` while calculating the source
  rank. This produced the wrong predecessor and triggered a
  `-Wsequence-point` warning.
- **Incorrect ring predecessor:** the predecessor is
  `(rank - 1 + size) % size`, not an assignment based on `size`.
- **Inconsistent datatype use:** the send used the committed contiguous
  `MPI_INT` datatype, while the receive used `MPI_BYTE`. Both sides now use
  one instance of the committed datatype.
- **Potential blocking-send deadlock:** the second ring exchange used
  blocking `MPI_Send` before `MPI_Recv` on every rank. Even though a small
  message may fit an implementation's eager buffer, that ordering is not
  reliable for all MPI configurations. Even ranks now send first and odd
  ranks receive first.
- **Datatype lifetime and result verification:** the derived datatype is freed
  after communication, and the received two-integer payload is checked before
  the program signs off.

## Reproduction and validation

The original programs were compiled with:

```text
mpicc -std=c11 -Wall -Wextra -Wpedantic -g -O1 \
  -fsanitize=address,undefined example_1.c -o example_1.before
mpicc -std=c11 -Wall -Wextra -Wpedantic -g -O1 \
  -fsanitize=address,undefined example_2.c -o example_2.before
```

The original `example_1` and `example_2` both timed out after 8 seconds with
two MPI processes. Compilation also reported the unused variable in
`example_1` and the sequence-point warning in `example_2`.

After the fixes, the same flags were used and the programs were run with:

```text
LSAN_OPTIONS=detect_leaks=0 \
  mpirun -n 2 ./example_1.after
LSAN_OPTIONS=detect_leaks=0 \
  mpirun -n 4 ./example_1.after

LSAN_OPTIONS=detect_leaks=0 \
  mpirun -n 2 ./example_2.after
LSAN_OPTIONS=detect_leaks=0 \
  mpirun -n 3 ./example_2.after
LSAN_OPTIONS=detect_leaks=0 \
  mpirun -n 4 ./example_2.after
```

All corrected runs completed successfully with no compiler warnings, AddressSanitizer
errors, or UndefinedBehaviorSanitizer errors. The payload checks also passed.

When LeakSanitizer was enabled without `LSAN_OPTIONS=detect_leaks=0`, Open MPI
reported allocations originating inside `libmpi.so` during runtime teardown.
Those are external MPI-runtime allocations, not allocations made by either
example. LeakSanitizer was disabled only for that known runtime noise; AddressSanitizer
and UndefinedBehaviorSanitizer remained enabled.
