unset key
set datafile separator ","
set term pngcairo

set view map
set tic scale 0
set cbrange [1:6]
set palette gray negative

if (!exists("numnodes")) numnodes=1.5

set xrange [-0.5:numnodes]
set yrange [-0.5:8.5]
set xtics border
set xtics("1" 0, "2" 1, "3" 2, "4" 3, "5" 4, "6" 5, "7" 6)
set ytics("BT" 8, "CG" 7, "DC" 6, "EP" 5, "FT" 4, "IS" 3, "MG" 2, "SP" 1, "UA" 0)

if (!exists("infile")) infile='numa_motivation.csv'
if (!exists("outfile")) outfile='muma_notivation.png'

set output outfile
splot infile matrix with image

