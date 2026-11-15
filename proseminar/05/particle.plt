set terminal gif animate delay 5 # set gif to animate in a frame delay of 5 ms
set output 'output.gif' # write to the file output.gif

set style line 2 lc rgb 'black' pt 7 # set line to be a filled circle of color black
stats 'data.dat' nooutput # read data from data.dat file 
stats 'data.dat' using 1 nooutput
xmin = STATS_min
xmax = STATS_max
stats 'data.dat' using 2 nooutput
ymin = STATS_min
ymax = STATS_max
stats 'data.dat' using 3 nooutput
zmin = STATS_min
zmax = STATS_max
set xrange [xmin:xmax] # show the complete range of x coordinates
set yrange [ymin:ymax] # show the complete range of y coordinates
set zrange [zmin:zmax] # show the complete range of z coordinates

stats 'data.dat' nooutput
do for [i=1:int(STATS_blocks)-1] {
   splot 'data.dat' index (i-1) with points ls 2 ps 0.4 # for each datapoint plot the point
}