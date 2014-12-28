set datafile separator ","
set term pngcairo

set yrange[0:9]
set xtic rotate by -45
set xtics("BT" 0, "CG" 1, "DC" 2, "EP" 3, "FT" 4, "IS" 5, "MG" 6, "SP" 7, "UA" 8, "Average" 9)
set grid ytics

set style data histogram
set style histogram cluster gap 4
set style fill solid border -1

if (!exists("infile")) infile='results.csv'
if (!exists("outfile")) outfile='results.png'

set output outfile
plot infile u 1 t col, '' u 5 t col, '' u 2 t col, '' u 6 t col, '' u 3 t col, '' u 7 t col, '' u 4 t col, '' u 8 t col

