#!/usr/bin/env sh

#adjust threads per node to max threasd per node so bench can generate up to this mant threads per run (can be used to reduce length of run)
BENCH_TIME=4.0
THREADS_PER_NODE=44
REPEAT_COUNT=9
PAUSE_CNT=1
rm -rf stressor_bins
mkdir stressor_bins
for DRNG_OP in "RDRAND" "RDSEED"; do
    for SAMPLE_WIDTH in "16" "32" "64"; do
        g++ drng_tput.cpp -DREPEAT_COUNT=${REPEAT_COUNT} -DDRNG_OP_TYPE_STR=\"${DRNG_OP}\" -DBENCH_TIME=${BENCH_TIME} -DTHREADS_PER_NODE=${THREADS_PER_NODE} -DDRNG_OP=${DRNG_OP} -DSAMPLE_WIDTH=${SAMPLE_WIDTH} -std=c++1z -mrdseed -mrdrnd -pthread -o ./stressor_bins/stress.drng.instr-${DRNG_OP}.width-${SAMPLE_WIDTH}.bin
    done
done

echo "OP,duration,width,latency,num_threads,tid,succeed_cnt,success_rate,success_bit_rate,failure_cnt,failure_rate,failure_bit_rate" > drngs_stress.csv
for DRNG_OP in "RDRAND" "RDSEED"; do
    for SAMPLE_WIDTH in "16" "32" "64"; do
        numactl --cpunodebind=0 ./stressor_bins/stress.drng.instr-${DRNG_OP}.width-${SAMPLE_WIDTH}.bin ${PAUSE_CNT} >> drngs_stress.csv
    done
done
