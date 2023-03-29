set title "Performance"
set xlabel "n^{th} fibonacci number"
set ylabel "Time(ns)"
set terminal png font "Times_New_Roman, 12"
set output "out_mul.png"
set key left

FILES = system("ls -1 *.txt")

plot for [data in FILES]\
data using 4 with linespoints linewidth 2 title substr(data, 0, strlen(data) - strlen(".txt"))\
