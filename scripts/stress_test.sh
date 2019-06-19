#!/usr/bin/env sh
TP=$(for file in /sys/devices/system/cpu/cpu[0-9]*/topology/thread_siblings_list; do echo -n "$file "; cat $file; done |sort -k2 -n | cut -d' ' -f2 | uniq)
BIN_DIR=/home/yshalab2/work/isca19/cc-fun/build/channels/pp-l1d
PP_L1D_SEND=${BIN_DIR}/pp-l1d-send
PP_L1D_RECV=${BIN_DIR}/pp-l1d-recv
for pair in $TP; do
    T1=$(echo ${pair} | cut -d, -f1)
    T2=$(echo ${pair} | cut -d, -f2)
    SEND_FILE=SEND.${T1}-${T2}.txt
    RECV_FILE=RECV.${T1}-${T2}.txt
    touch ${SEND_FILE}
    touch ${RECV_FILE}
    for x in seq 0 100; do
        DATA=$(head -c 128 /dev/urandom | tr -dc 'a-zA-Z0-9~!@#$%^&*_-' | fold -w 100 | head -n 1)
        echo ${DATA} >> ${SEND_FILE}
    done
    echo "exit" >> ${SEND_FILE}

    echo "sudo taskset -c ${T1} ${PP_L1D_SEND} > ${RECV_FILE}"
    sudo taskset -c ${T1} ${PP_L1D_SEND} > ${RECV_FILE} &
    SEND_PID=$!
    echo "sudo taskset -c ${T1} ${PP_L1D_RECV} < ${SEND_FILE}"
    sudo taskset -c ${T2} ${PP_L1D_RECV} < ${SEND_FILE}
    sudo kill -9 ${SEND_PID}
    cat ${RECV_DATA}
done
