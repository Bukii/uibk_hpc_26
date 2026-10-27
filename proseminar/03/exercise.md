# Assignment 03

## Exercise 1

- Elaborate on the settings/parameters you used and what kind of errors they should detect!
    - `CFLAGS=-std=gnu11 -Wall -Wextra -fsanitize=address,undefined -g`
    - `-Wall` enables all warnings about constructions
    - `-Wextra` enables extra warning flags that are not enabled by Wall
    - `-pedantic` issues all the warnings demanded by strict ISO C and ISO C++
    - `-fsanitize` enables sanitizers (leaks, race conditions, undefined behaviour)
    - `-g` shows you the exact stack trace for these sanitize errors
- Report any bugs or issues that were found and attempt to fix them
```bash
example_1.c: In function 'main':
example_1.c:13:9: warning: unused variable 'i' [-Wunused-variable]
   13 |     int i;
      |         ^
example_2.c: In function 'main':
example_2.c:32:28: warning: operation on 'rank' may be undefined [-Wsequence-point]
   32 |                      (rank = 1 + size) % size, 123, MPI_COMM_WORLD, &status);
      |                      ~~~~~~^~~~~~~~~~~
```
- Is there anything else that can be done to highlight or avoid bugs?
    - asd
- 