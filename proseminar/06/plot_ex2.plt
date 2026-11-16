# Plots the CSV written by benchmark_ex2.slurm (uniform vs. corner).
# Usage: gnuplot -e "data='nbody_ex2_<jobid>.csv'" plot_ex2.plt
#        (add n=2500 inside -e to plot another particle count; default 5000)
# Writes ex2_scaling.svg: rows = wall time, efficiency, load imbalance;
#                         columns = theta.

if (!exists("data")) data = 'nbody_ex2.csv'
if (!exists("n")) n = 5000
set datafile separator ','
# CSV columns: 1 dist, 2 theta, 3 particles, 4 ranks, 6 time, 10 imbalance, 12 efficiency
sel(d, th, col) = (strcol(1) eq d && abs($2 - th) < 1e-9 && $3 == n) ? column(col) : NaN
thetas = "0 0.5 1"
dists = "uniform corner"
name(th) = th == 0 ? "direct (θ = 0)" : sprintf("Barnes–Hut θ = %g", th)

set terminal svg size 1200,950 font 'sans,11'
set output 'ex2_scaling.svg'
set multiplot layout 3,3 title sprintf("uniform vs. corner distribution, N = %d", n)
set logscale x 2
set xrange [1:96]
set xtics (1, 2, 4, 6, 12, 24, 48, 96)
set grid
set style data linespoints

# Row 1: wall time (log scale).
do for [th in thetas] {
    set title name(th + 0)
    set logscale y 10
    set format y '%g'
    set ylabel 'wall time [s]'
    set key top right
    plot for [d in dists] data using 4:(sel(d, th + 0, 6)) pt 7 ps 0.6 title d
}
# Row 2: parallel efficiency relative to 1 rank of the same distribution.
unset title
do for [th in thetas] {
    unset logscale y
    set yrange [0:1.1]
    set ylabel 'efficiency'
    set key bottom left
    plot for [d in dists] data using 4:(sel(d, th + 0, 12)) pt 7 ps 0.6 title d
}
# Row 3: load imbalance of the force phase (slowest rank / average rank).
do for [th in thetas] {
    set yrange [0.95:*]
    set ylabel 'imbalance (max / avg)'
    set xlabel 'MPI ranks'
    set key top left
    plot for [d in dists] data using 4:(sel(d, th + 0, 10)) pt 7 ps 0.6 title d, \
         1 with lines dt 2 lc 'gray' title 'balanced'
}
unset multiplot
