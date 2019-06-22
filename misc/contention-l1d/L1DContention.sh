#!/usr/bin/env bash
CONTENTION_BINS=/isca19/bins
MEASURE_L1D_CON=${CONTENTION_BINS}/measure-l1d-contention
MODE=points #2
CORE=${TSEND}
OTHER=${TRECV}
while true
do
    ${MEASURE_L1D_CON} > contention.dat
    clear
    cat contention.dat | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index (Shared with Thread-${OTHER})\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} title \"Thread-${CORE} L1D Access Time (cycles)\"";
done
