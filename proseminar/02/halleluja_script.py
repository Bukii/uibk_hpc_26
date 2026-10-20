import matplotlib.pyplot as plt

ranks = [1, 6, 12, 24, 48, 96]
mpi_times = [23.311672, 4.296937, 2.054920, 1.122981, 0.578822, 0.324920]

sequential_time = 23.183330

plt.plot(ranks, mpi_times, marker="o", label="MPI")
plt.axhline(sequential_time, linestyle="--", label="Sequential")

plt.xlabel("Number of MPI ranks")
plt.ylabel("Execution time (seconds)")
plt.title("Sequential vs MPI performance")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig("performance.png", dpi=300)
plt.show()