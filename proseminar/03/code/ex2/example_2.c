#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
	int rank;
	int size;
	int next;
	int previous;
	int received_correctly;
	int sBuf[2] = { 1, 2 };
	int rBuf[2];
	MPI_Status status;
	MPI_Datatype newType;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	next = (rank + 1) % size;
	previous = (rank - 1 + size) % size;

	// Enough tasks?
	if(size < 2) {
		printf("This test needs at least 2 processes ! \n");
		MPI_Finalize();
		return 1;
	}

	// Say hello
	printf("Hello, I am rank %d of %d processes. \n", rank, size);

	//) Create a datatype
	MPI_Type_contiguous(2, MPI_INT, &newType);
	MPI_Type_commit(&newType);

	// 2) Use MPI Sendrecv to perform a ring communication
	MPI_Sendrecv(sBuf, 1, newType, next, 123, rBuf, 1, newType, previous, 123,
	             MPI_COMM_WORLD, &status);

	// 3) Use MPI Send and MPI Recv to perform a ring communication
	if(rank % 2 == 0) {
		MPI_Send(sBuf, 1, newType, next, 456, MPI_COMM_WORLD);
		MPI_Recv(rBuf, 1, newType, previous, 456, MPI_COMM_WORLD, &status);
	} else {
		MPI_Recv(rBuf, 1, newType, previous, 456, MPI_COMM_WORLD, &status);
		MPI_Send(sBuf, 1, newType, next, 456, MPI_COMM_WORLD);
	}

	MPI_Type_free(&newType);
	received_correctly = rBuf[0] == sBuf[0] && rBuf[1] == sBuf[1];
	if(!received_correctly) {
		fprintf(stderr, "Rank %d received incorrect data: {%d, %d}\n", rank, rBuf[0],
		        rBuf[1]);
		MPI_Abort(MPI_COMM_WORLD, 1);
	}

	// Say bye bye
	printf("Signing off, rank %d. \n", rank);

	MPI_Finalize();

	return 0;
}
