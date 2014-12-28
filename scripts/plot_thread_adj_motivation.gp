unset key
set datafile separator ","
set term pngcairo

set view map
set tic scale 0
set cbrange [1:3]
set palette gray negative

set xrange [-0.5:8.5]
set yrange [-0.5:8.5]
set xtics border
set xtics("BT" 0, "CG" 1, "DC" 2, "EP" 3, "FT" 4, "IS" 5, "MG" 6, "SP" 7, "UA" 8)
set ytics("BT" 8, "CG" 7, "DC" 6, "EP" 5, "FT" 4, "IS" 3, "MG" 2, "SP" 1, "UA" 0)

if (!exists("infile")) infile='thread_adj_motivation.csv'
if (!exists("outfile")) outfile='thread_adj_motivation.png'

set output outfile
splot infile matrix with image

