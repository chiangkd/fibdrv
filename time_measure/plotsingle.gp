set title "Performance"
set xlabel "n^{th} fibonacci number"
set ylabel "Time(ns)"
set terminal png font "Times_New_Roman, 12"
set output "out.png"
set key left

FILE = "bignum_iter.txt"

plot \
FILE using 3 with linespoints linewidth 2 title "user level", \
FILE using 4 with linespoints linewidth 2 title "kernel level", \
FILE using 5 with linespoints linewidth 2 title "kernel to user", \