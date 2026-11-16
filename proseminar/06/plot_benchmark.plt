# Plots the CSV written by benchmark.slurm.
# Usage: gnuplot -e "data='nbody_bench_<jobid>.csv'" plot_benchmark.plt
# Writes scaling.svg (speedup + efficiency) and breakdown.svg (N=5000 timings).

if (!exists("data")) data = 'nbody_bench_partial.csv'
set datafile separator ','
# CSV columns: 1 theta, 2 particles, 3 ranks, 5 time, 6 build, 7 force,
#              8 comm, 9 speedup, 10 efficiency
sel(th, n, col) = (abs($1 - th) < 1e-9 && $2 == n) ? column(col) : NaN
thetas = "0 0.5 1"
sizes = "1000 2500 5000 10000"
name(th) = th == 0 ? "direct (θ = 0)" : sprintf("Barnes–Hut θ = %g", th)

set logscale x 2
set xrange [1:96]
set xtics (1, 2, 4, 6, 12, 24, 48, 96)
set grid
set key top left
set style data linespoints

# ---- Speedup (top row) and efficiency (bottom row), one column per theta ----
set terminal svg size 1200,700 font 'sans,11'
set output 'scaling.svg'
set multiplot layout 2,3
do for [th in thetas] {
    set title name(th + 0)
    set logscale y 2
    set yrange [1:96]
    set ytics (1, 2, 4, 8, 16, 32, 64, 96)
    set ylabel 'speedup'
    set xlabel ''
    plot for [n in sizes] data using 3:(sel(th + 0, n + 0, 9)) pt 7 ps 0.6 title 'N = '.n, \
         x with lines dt 2 lc 'gray' title 'ideal'
}
do for [th in thetas] {
    unset title
    unset logscale y
    set yrange [0:1.05]
    set ytics auto
    set ylabel 'efficiency'
    set xlabel 'MPI ranks'
    set key bottom left
    plot for [n in sizes] data using 3:(sel(th + 0, n + 0, 10)) pt 7 ps 0.6 title 'N = '.n
}
unset multiplot

# ---- Where the time goes for N = 5000 (max over ranks per phase) ----
set terminal svg size 1200,380 font 'sans,11'
set output 'breakdown.svg'
set multiplot layout 1,3
set logscale y 10
set yrange [0.005:60]
set ytics auto
set format y '%g'
set ylabel 'time [s]'
set xlabel 'MPI ranks'
set key top right
do for [th in thetas] {
    set title name(th + 0).', N = 5000'
    plot data using 3:(sel(th + 0, 5000, 5)) pt 7 ps 0.6 lw 2 title 'total', \
         ''   using 3:(sel(th + 0, 5000, 7)) pt 7 ps 0.6 title 'force', \
         ''   using 3:(th + 0 > 0 ? sel(th + 0, 5000, 6) : NaN) pt 7 ps 0.6 title 'tree build', \
         ''   using 3:(sel(th + 0, 5000, 8)) pt 7 ps 0.6 title 'communication'
}
unset multiplot
