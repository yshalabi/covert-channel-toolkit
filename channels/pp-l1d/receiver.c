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
/*
 * Detects a bit by repeatedly measuring the access time of the addresses in the
 * probing set and counting the number of misses for the clock length of state->interval.
 *
 * If the the first_bit argument is true, relax the strict definition of "one" and try to
 * sync with the sender.
 */
typedef struct __attribute__((__packed__)) {
    unsigned char num;
    char msg[7];
    unsigned char ecc;
} cc_payload;
#define THRESHOLD_ONE 130
#define L1_HIT 74
void fill_packet(char * p, uint16_t * set_cycles, int * set_map)
{
    char payload[8] = {0,0,0,0,0,0,0,0};
    for(int i =0; i < 64; i++)
    {
        int set = set_map[i];
        int idx = (set/8);
        int bit = (set%8);
        int ones = 0;
        for(int s = 0; s < 256; s++){
            int sample = s*64 + i;
            if(set_cycles[sample] > L1_HIT) {
                ones++;
            }
        }
        if(ones > THRESHOLD_ONE)
            payload[idx] |= (1<<bit); 
    }
    if(payload[0] == 128 || payload[1] == 128)
        printf("good transmission.\n");
    if(payload[3] == 'z' || payload[4] == 'z')
        printf("good transmission.\n");
    memcpy((void*)p, (void*)payload, 8);
    return;
}

bool is_valid(char * p) 
{

    char num1 = p[0];
    char num2 = p[1];
    char num3 = p[2];
    char pl[2] = {0,0};
    for(int c = 0; c < 2; c++) {
        for(int b = 0; b < 4; b++) {
            int ones = 0;
            for(int i = 0; i < 3; i++) {
                if(p[c*3 + i] & (1<<b))
                    ones++;
            }
            if(ones >=2)
                pl[c] |= (1<<b);
        }
    }
    unsigned char ecc1 = p[6]; 
    unsigned char ECC   = pl[0] ^ pl[1] ^ 0xff; 
    unsigned char ecc2 = p[7];
    //unsigned int x = (ecc1 << 8) | ecc2;
    char s = 'z';
    char n = 128;
    char e = 0xff ^ s ^ n;
    char act[8] = {n,n,n,s,s,s,e,e};
    printf("[");
    for(int i =0; i < 8; i++)
    {
        unsigned char c = p[i];
        unsigned char exp = act[i];
        unsigned int x = (unsigned int) c;
        unsigned int x2 = (unsigned int) exp;
        printf("%x (%x)",x, x2);
        if(i != 7) printf(",");
    }
    printf("] -- ");
    printf("ECC: %x\n", ECC);
    return (ecc1 == ECC) || (ecc2 == ECC);
}

void dump_res(uint16_t* res, int * map) 
{
    int results[64];
    for(int i = 0; i < 64; i++){
        int set = map[i];
        int cyc = res[i];
        results[set] = cyc;
    }
    int set = 0;
    printf("-----------");
    for(int i = 0; i < 8; i++)
    {
        char x = 0;
        for(int j = 0; j < 8; j++)
        {
            int ones = 0;
            int set_idx = set++;
            for(int s = 0; s < 256; s++)
            {
                int sample_offset = s*64;
                if(res[map[set_idx] + sample_offset] > L1_HIT)
                    ones++;
            }

            if(ones > THRESHOLD_ONE)
                x |= (1<<j);
        }

        if(i == 0){
            uint16_t num = (uint16_t) x;
            printf("[%d,",x);
        }
        if(i >= 1 && i < 7)
            printf("%c,",x);
        if(i == 7){
            uint16_t num = (uint16_t) x;
            printf("%d]",x);
        }
    }
    printf("------\n");
}

#define SYNC_DELAY_FACTOR 28
#define NEXT_PERIOD_BOUNDARY (1<<(SYNC_DELAY_FACTOR+1))

void synchronize()
{
    uint64_t mask = (1 << 44)-1; 
    while(rdtscp() & mask > 0x1000)
        ;
}

int set_timings[64][256];
int set_threshold[64];
char payload[8];
void measure_thresholds(uint16_t * results, int * rmap)
{
    for(int i = 0; i < 64; i++)
    {
        for(int j = 0; j < 256; j++) set_timings[i][j] = 0;
        for(int s = 0; s < 256; s++) {
            int cycles = results[s*64 + rmap[i]];
            if(cycles < 256 && cycles >0)
                set_timings[i][cycles]++;
        }
    }
    for(int i = 0; i < 64; i++) {
        int sum = 0;
        for(int j = 0; j < 256; j++) {
            sum+=set_timings[i][j];
            if(sum < 128)
                continue;
            set_threshold[i] = j;
            break;
        }
    }
    for(int c = 0; c < 8; c++){
        payload[c] = 0;
        for(int b = 0; b < 8; b++) {
            int i = c*8 + b;
            if(set_threshold[i] > 100)
                payload[c] |= (1<<b);
        }
    }
}

int main(int argc, char **argv)
{
    char msg[7];
    msg[6] = 0;
    char packet[8] = {0,0,0,0,0,0,0,0};
    uint64_t next_period;
    int i;
    l1pp_t l1 = l1_prepare();
    int nsets = l1_getmonitoredset(l1, NULL, 0);
    int *map = calloc(nsets, sizeof(int));
    l1_getmonitoredset(l1, map, nsets);

    int rmap[64];
    for(int i =0;i<64;i++) rmap[i] = -1;
    for(int i =0; i < nsets; i++) rmap[map[i]] = i;

    int samples = 256;
    uint16_t *results = calloc(samples * nsets, sizeof(uint16_t));
    for (int i = 0; i < samples * nsets; i+= 4096/sizeof(uint16_t))
        results[i] = 1;
    //for(int i =0; i < nsets; i++) res[i] = 1; 
  
    //delayloop(3000000000U);
    //l1_repeatedprobe(l1, samples, res, 0);
   // dump_res(res,map);
   // return 0;

    while(1) {

        synchronize();
        for(int i = 0; i < 128; i++)
        {
            uint16_t * res = results + nsets*i*2;
            //prime
            l1_probe(l1, res);
            res = results + nsets*(2*i) + nsets;
            l1_bprobe(l1, res);
        }

        measure_thresholds(results,rmap);
        for(int i =0; i < 8; i++) printf("-%c-",payload[i]);
        printf("\n");
        
        for(int i =0; i < 1000*1000*10; i++)
            asm("pause");
        
        //Fill the packet
        fill_packet(&packet, results, rmap);
        //if valid output
        if(is_valid(&packet) == false){
            continue;
        } else {
            sched_yield();
        }
        dump_res(results,rmap);
        printf("VALID TRANSMISSION\n");
        //print contents
        char * payload_msg = packet + 1;
        for(i =0; i < 6; i++)
            msg[i] = payload_msg[i];
        memset(&packet,0,8);
    }
    return 0;
}


