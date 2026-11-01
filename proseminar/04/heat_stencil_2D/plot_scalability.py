#!/usr/bin/env python3
"""Plot strong-scaling results from heat_stencil_2D_mpi.log."""

import re
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


HERE = Path(__file__).resolve().parent
LOG = HERE / "heat_stencil_2D_mpi.log"
OUTPUT = HERE / "heat_stencil_2D_scalability.svg"
METRICS_OUTPUT = HERE / "heat_stencil_2D_metrics.svg"


def read_results():
    ranks = []
    times = []
    current_ranks = None
    for line in LOG.read_text().splitlines():
        match = re.match(r"MPI ranks: (\d+)", line)
        if match:
            current_ranks = int(match.group(1))
            continue
        match = re.match(r"Wall time: ([0-9.]+) seconds", line)
        if match and current_ranks is not None:
            ranks.append(current_ranks)
            times.append(float(match.group(1)))
            current_ranks = None

    if not ranks:
        raise RuntimeError(f"No timing results found in {LOG}")
    return ranks, times


def main():
    ranks, times = read_results()
    baseline = times[0]
    speedup = [baseline / time for time in times]
    efficiency = [value / rank for value, rank in zip(speedup, ranks)]
    cells = 768 * 768
    timesteps = 76800
    mlups = [cells * timesteps / time / 1e6 for time in times]

    figure, axes = plt.subplots(1, 3, figsize=(14, 4))

    axes[0].plot(ranks, times, "o-", color="#00538f")
    axes[0].set_ylabel("wall time (s)")
    axes[0].set_title("Runtime")

    axes[1].plot(ranks, speedup, "o-", label="measured", color="#00538f")
    axes[1].plot(ranks, ranks, "--", label="ideal", color="#888888")
    axes[1].set_ylabel("speedup")
    axes[1].set_title("Strong scaling speedup")
    axes[1].legend()

    axes[2].plot(ranks, efficiency, "o-", color="#b66d00")
    axes[2].axhline(1.0, linestyle="--", color="#888888")
    axes[2].set_ylabel("efficiency")
    axes[2].set_title("Parallel efficiency")
    axes[2].set_ylim(0, 1.1)

    for axis in axes:
        axis.set_xlabel("MPI ranks")
        axis.set_xticks(ranks)
        axis.grid(axis="y", alpha=0.25)
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)

    figure.suptitle("Heat stencil 2D: N=768, T=76800")
    figure.tight_layout()
    figure.savefig(OUTPUT, format="svg", bbox_inches="tight")

    metrics_figure, metrics_axes = plt.subplots(1, 2, figsize=(9, 4))
    metrics_axes[0].plot(ranks, mlups, "o-", color="#00538f")
    metrics_axes[0].set_title("Domain-specific metric")
    metrics_axes[0].set_ylabel("MLUPS")
    metrics_axes[1].plot(ranks, efficiency, "o-", color="#b66d00")
    metrics_axes[1].set_title("Domain-independent metric")
    metrics_axes[1].set_ylabel("parallel efficiency")
    metrics_axes[1].set_ylim(0, 1.1)
    for axis in metrics_axes:
        axis.set_xlabel("MPI ranks")
        axis.set_xticks(ranks)
        axis.grid(axis="y", alpha=0.25)
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    metrics_figure.suptitle("Heat stencil 2D: performance metrics")
    metrics_figure.tight_layout()
    metrics_figure.savefig(METRICS_OUTPUT, format="svg", bbox_inches="tight")
    plt.close(metrics_figure)

    print("ranks,time,speedup,efficiency")
    for rank, time, value, eff, rate in zip(
        ranks, times, speedup, efficiency, mlups
    ):
        print(f"{rank},{time:.6f},{value:.3f},{eff:.3f},{rate:.3f}")
    print(f"Wrote {OUTPUT} and {METRICS_OUTPUT}")


if __name__ == "__main__":
    main()
