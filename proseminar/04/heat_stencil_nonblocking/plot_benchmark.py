#!/usr/bin/env python3
"""Create plots from the non-blocking heat-stencil benchmark CSV."""

import csv
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


HERE = Path(__file__).resolve().parent
DEFAULT_CSV = HERE / "heat_stencil_nonblocking_406981.csv"
RUNTIME_OUTPUT = HERE / "heat_stencil_nonblocking_runtime.svg"
SCALING_OUTPUT = HERE / "heat_stencil_nonblocking_scaling.svg"


def read_results(path):
    with path.open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise RuntimeError(f"No benchmark rows found in {path}")
    if any(row["verification"] != "OK" for row in rows):
        raise RuntimeError("The benchmark contains a failed verification.")
    for row in rows:
        row["N"] = int(row["N"])
        row["ranks"] = int(row["ranks"])
        row["wall_time_seconds"] = float(row["wall_time_seconds"])
    return rows


def style_axis(axis):
    axis.grid(axis="y", alpha=0.25)
    axis.spines["top"].set_visible(False)
    axis.spines["right"].set_visible(False)


def main():
    csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CSV
    rows = read_results(csv_path)
    stencils = ["1D", "2D"]
    sizes = sorted({row["N"] for row in rows})
    colors = {"1D": "#00538f", "2D": "#b66d00"}

    figure, axes = plt.subplots(1, len(sizes), figsize=(14, 4), sharey=False)
    for axis, size in zip(axes, sizes):
        for stencil in stencils:
            values = [
                row
                for row in rows
                if row["stencil"] == stencil and row["N"] == size
            ]
            values.sort(key=lambda row: row["ranks"])
            axis.plot(
                [row["ranks"] for row in values],
                [row["wall_time_seconds"] for row in values],
                "o-",
                label=stencil,
                color=colors[stencil],
            )
        axis.set_title(f"N={size}")
        axis.set_xlabel("MPI ranks")
        axis.set_xticks(sorted({row["ranks"] for row in rows}))
        axis.set_yscale("log")
        axis.set_ylabel("wall time (s)")
        style_axis(axis)
    axes[0].legend()
    figure.suptitle("Non-blocking heat stencils: runtime comparison")
    figure.tight_layout()
    figure.savefig(RUNTIME_OUTPUT, format="svg", bbox_inches="tight")
    plt.close(figure)

    figure, axes = plt.subplots(2, 2, figsize=(10, 7), sharex="col")
    for column, stencil in enumerate(stencils):
        for size in sizes:
            values = [
                row
                for row in rows
                if row["stencil"] == stencil and row["N"] == size
            ]
            values.sort(key=lambda row: row["ranks"])
            baseline = values[0]["wall_time_seconds"]
            ranks = [row["ranks"] for row in values]
            speedup = [baseline / row["wall_time_seconds"] for row in values]
            efficiency = [value / rank for value, rank in zip(speedup, ranks)]
            axes[0, column].plot(ranks, speedup, "o-", label=f"N={size}")
            axes[1, column].plot(ranks, efficiency, "o-", label=f"N={size}")
        axes[0, column].plot(ranks, ranks, "--", color="#888888", label="ideal")
        axes[0, column].set_title(f"{stencil} stencil")
        axes[0, column].set_ylabel("speedup")
        axes[1, column].set_ylabel("parallel efficiency")
        axes[1, column].set_ylim(0, 1.1)
        axes[1, column].set_xlabel("MPI ranks")
        axes[0, column].legend(fontsize="small")
        axes[1, column].legend(fontsize="small")
        for row in range(2):
            axes[row, column].set_xticks(sorted({r["ranks"] for r in rows}))
            style_axis(axes[row, column])
    figure.suptitle("Non-blocking heat stencils: strong-scaling metrics")
    figure.tight_layout()
    figure.savefig(SCALING_OUTPUT, format="svg", bbox_inches="tight")
    plt.close(figure)

    print(f"Wrote {RUNTIME_OUTPUT} and {SCALING_OUTPUT}")


if __name__ == "__main__":
    main()
