#!/usr/bin/env bash
CONTENTION_BINS=.
MEASURE_L1D_CON=${CONTENTION_BINS}/measure-l1d-contention
MODE=lines
MODE=points #2
CORE=$0
if [[ "$#" -eq "0" ]]; then
	echo "This script visualizes the contention on an L1D cache. Please suply the core number"
	echo "Usage: $0 <Core_Number>"
	exit
fi
CORE=$1
T1=$(cat /sys//devices/system/cpu/cpu${CORE}/topology/thread_siblings_list | cut -d, -f1)
T2=$(cat /sys//devices/system/cpu/cpu${CORE}/topology/thread_siblings_list | cut -d, -f2)
echo "Monitoring thread number: $CORE"
if [[ "${CORE}" -eq "${T1}" ]]; then
	echo "L1D shared with thread ${T2}"
	OTHER=${T2}
else
	echo "L1D shared with thread ${T1}"
	OTHER=${T1}
fi

while true
do
    sudo taskset -c ${CORE} ${MEASURE_L1D_CON} > contention.dat
    clear
    for i in contention.dat;
    do
    	cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index (Shared with T${OTHER})\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} title \"T${CORE} L1D Access Time (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} pt '@' title \"Tiume (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120]; plot '-' using 1:3 with ${MODE} title \"Tiume (cycles)\"";
        #cat $i | gnuplot -p -e "set terminal dumb size 120, 30; set xlabel \"L1D Set Index\"; set yrange [30:120];  set autoscale; plot '-' using 1:3 with ${MODE} title \"Tiume (cycles)\"";

        #| grep --color=always '*';
    done
done
