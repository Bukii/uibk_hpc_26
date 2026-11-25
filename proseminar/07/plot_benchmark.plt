# Plots the CSV written by benchmark.slurm.
# Usage: gnuplot -e "data='nbody_rma_<jobid>.csv'" plot_benchmark.plt
#        (add n=1000 inside -e to change the size in comm.svg; default 5000)
# Writes scaling.svg (speedup + efficiency) and comm.svg (total and
# communication time).

if (!exists("data")) data = 'nbody_rma.csv'
if (!exists("n")) n = 5000
set datafile separator ','
# CSV columns: 1 mode, 2 theta, 3 particles, 4 ranks, 6 time, 7 build, 8 force,
#              9 comm, 10 speedup, 11 efficiency
sel(m, th, size, col) = \
    (strcol(1) eq m && abs($2 - th) < 1e-9 && $3 == size) ? column(col) : NaN
thetas = "0 0.5 1"
sizes = "1000 2500 5000 10000"
name(th) = th == 0 ? "direct (θ = 0)" : sprintf("Barnes–Hut θ = %g", th)

set logscale x 2
set xrange [1:96]
set xtics (1, 2, 4, 6, 12, 24, 48, 96)
set grid
set style data linespoints

# ---- Speedup and efficiency: solid = two-sided, dashed = one-sided ----
set terminal svg size 1200,800 font 'sans,11'
set output 'scaling.svg'
set multiplot layout 2,3
do for [th in thetas] {
    set title name(th + 0)
    set logscale y 2
    set yrange [0.5:96]
    set ytics (1, 2, 4, 8, 16, 32, 64, 96)
    set ylabel 'speedup'
    set xlabel ''
    set key top left
    plot for [i=1:words(sizes)] data \
             using 4:(sel("allgatherv", th + 0, word(sizes, i) + 0, 10)) \
             lc i dt 1 pt 7 ps 0.6 title 'allgatherv, N = '.word(sizes, i), \
         for [i=1:words(sizes)] data \
             using 4:(sel("put", th + 0, word(sizes, i) + 0, 10)) \
             lc i dt 2 pt 6 ps 0.8 title 'put, N = '.word(sizes, i), \
         x with lines dt 3 lc 'gray' title 'ideal'
}
do for [th in thetas] {
    unset title
    unset logscale y
    set yrange [0:1.1]
    set ytics auto
    set ylabel 'efficiency'
    set xlabel 'MPI ranks'
    set key bottom left
    plot for [i=1:words(sizes)] data \
             using 4:(sel("allgatherv", th + 0, word(sizes, i) + 0, 11)) \
             lc i dt 1 pt 7 ps 0.6 title 'allgatherv, N = '.word(sizes, i), \
         for [i=1:words(sizes)] data \
             using 4:(sel("put", th + 0, word(sizes, i) + 0, 11)) \
             lc i dt 2 pt 6 ps 0.8 title 'put, N = '.word(sizes, i)
}
unset multiplot

# ---- Total and communication time for one problem size ----
set terminal svg size 1200,420 font 'sans,11'
set output 'comm.svg'
set multiplot layout 1,3
set logscale y 10
set yrange [*:*]
set ytics auto
set format y '%g'
set ylabel 'time [s]'
set xlabel 'MPI ranks'
set key top right
do for [th in thetas] {
    set title sprintf("%s, N = %d", name(th + 0), n)
    plot data using 4:(sel("allgatherv", th + 0, n, 6)) lc 1 dt 1 pt 7 title 'total, allgatherv', \
         data using 4:(sel("put", th + 0, n, 6)) lc 1 dt 2 pt 6 title 'total, put', \
         data using 4:(sel("allgatherv", th + 0, n, 9)) lc 2 dt 1 pt 7 title 'comm, allgatherv', \
         data using 4:(sel("put", th + 0, n, 9)) lc 2 dt 2 pt 6 title 'comm, put'
}
unset multiplot
