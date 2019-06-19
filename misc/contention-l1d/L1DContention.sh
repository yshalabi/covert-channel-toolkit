#!/usr/bin/env bash
CONTENTION_BINS=../../build/misc/contention-l1d
MEASURE_L1D_CON=${CONTENTION_BINS}/measure-l1d-contention
MODE=points #2
MODE=lines
while true
do
    sudo taskset -c 0 ${MEASURE_L1D_CON} > contention.dat
    clear
    for i in contention.dat;
    do
        cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} title \"Time (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} pt '@' title \"Tiume (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} title \"Tiume (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120];  set autoscale; plot '-' using 1:3 with ${MODE} title \"Tiume (cycles)\"";

        #| grep --color=always '*';
    done
done
