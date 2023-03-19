set title "Performance"
set xlabel "n^{th} fibonacci number"
set ylabel "Time(ns)"
set terminal png font "Times_New_Roman, 12"
set output "performance.png"
set key left

plot \
"measure_time.txt" using 3 with linespoints linewidth 2 title "normal iter.",\
"measure_time_fd_iter.txt" using 3 with linespoints linewidth 2 title "fast doubling iter.",\
"measure_time_fd_recur.txt" using 3 with linespoints linewidth 2 title "fast doubling recur.",\
