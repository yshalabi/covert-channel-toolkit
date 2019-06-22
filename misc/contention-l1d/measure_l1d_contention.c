#include <sched.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "l1.h"
#include "low.h"

#define FIRST_CHAR_INDEX 2
#define LAST_CHAR_INDEX 6
#define SAMPLE_COUNT (1024*1024)

uint64_t set_timings[64][256];
void decode_thresholds(uint16_t * results, int * rmap)
{
    int set_threshold[64];
    int set_samples[64];
    for(int i = 0; i < 64; i++)
    {
        set_samples[i] = 0;
        set_threshold[i] = 0;
        for(int j = 0; j < 256; j++) set_timings[i][j] = 0;
        for(int s = 0; s < SAMPLE_COUNT; s++) {
            int cycles = results[s*64 + rmap[i]];
            if(cycles < 256 && cycles >5) {
                set_timings[i][cycles]++;
                set_samples[i]++;
            }
        }
        for(int c = 0; c < 256; c++)
        {
            if(set_timings[i][c] == 0) continue;
            //printf("set_timings[set=%d][cycles=%d]=%d\n", i,c, set_timings[i][c]);
        }
    }
    for(int i = 0; i < 64; i++) {
        uint64_t sum = 0;
        int half = set_samples[i]/2;
        //printf("HP Set(%d) = %d\n",i,half);
        set_threshold[i] = 256;
        for(int j = 0; j < 256; j++) {
            sum+=set_timings[i][j];
            if(sum < half)
                continue;
            set_threshold[i] = j;
            break;
        }
    }
    int set = 0;
    for(int c = 0; c < 8; c++){
        for(int b = 0; b < 8; b++) {
            int i = c*8 + b;
            printf("%d\tS%d\t%d\n", set, set++, set_threshold[i]);
        }
    }
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    char msg[7];
    l1pp_t l1 = l1_prepare();
    int nsets = l1_getmonitoredset(l1, NULL, 0);
    int *map = calloc(nsets, sizeof(int));
    l1_getmonitoredset(l1, map, nsets);

    int rmap[64];
    for(int i =0;i<64;i++) rmap[i] = -1;
    for(int i =0; i < nsets; i++) rmap[map[i]] = i;

    uint16_t * results;
    for(int r = 0; r < 12; r++)
    {
        int samples = SAMPLE_COUNT;
        results = calloc(samples * nsets, sizeof(uint16_t));
        for (int i = 0; i < samples * nsets; i+= 4096/sizeof(uint16_t))
            results[i] = 1;

        for(int i = 0; i < 128; i++)
        {
            uint16_t * res = results + nsets*i*2;
            l1_probe(l1, res);
            res = results + nsets*(2*i) + nsets;
            l1_bprobe(l1, res);
        }

        for(int d = 0; d < 1000*1000; d++) asm("pause");
        
    }
    decode_thresholds(results,rmap);

    return 0;
}
