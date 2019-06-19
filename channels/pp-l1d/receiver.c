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
#include "crc16.h"

#define SIGNALING_THRESHOLD 120
#define FIRST_CHAR_INDEX 2
#define LAST_CHAR_INDEX 6

bool verify_crc16(char * p) 
{
    uint16_t crc = crc16_ccitt_init();
    for(int i =0; i < 8; i++)
        crc = crc16_ccitt_update(p[i], crc);
    crc = crc16_ccitt_finalize(crc);
    return (crc == 0);
}

int set_timings[64][256];
void decode_thresholds(uint16_t * results, int * rmap, char * dst)
{
    char payload[8] = {0,0,0,0,0,0,0,0};
    int set_threshold[64];
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
    int set = 0;
    for(int c = 0; c < 8; c++){
        payload[c] = 0;
        for(int b = 0; b < 8; b++) {
            int i = c*8 + b;
            //printf("SET %d = %d\n", set++, set_threshold[i]);
            if(set_threshold[i] > SIGNALING_THRESHOLD)
                payload[c] |= (1<<b);
        }
    }
    dst[0] = payload[0];
    dst[1] = payload[1];
    dst[2] = payload[2];
    dst[3] = payload[3];
    dst[4] = payload[4];
    dst[5] = payload[5];
    dst[6] = payload[6];
    dst[7] = payload[7];
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    char msg[7];
    msg[6] = 0;
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

    int next_msg = 0;
    uint16_t last_packet_num = 0xFFFF;
    char buf[128];
    char * pos = buf;
    while(1) {

        char packet[8] = {0,0,0,0,0,0,0,0};
        for(int i = 0; i < 128; i++)
        {
            uint16_t * res = results + nsets*i*2;
            l1_probe(l1, res);
            res = results + nsets*(2*i) + nsets;
            l1_bprobe(l1, res);
        }

        decode_thresholds(results,rmap, packet);

        bool crc_is_valid   = verify_crc16(packet);
        uint16_t packet_num = (uint16_t) packet[0];
        bool is_new_packet  = packet_num != last_packet_num;

        if(crc_is_valid && is_new_packet)
            last_packet_num = packet_num;
        else {
            sched_yield();
            continue;
        }

        //Move the packet contents into the buffer
        for(int i = FIRST_CHAR_INDEX; i < LAST_CHAR_INDEX; i++ ) {
            //A new line is the end of the message
            if(packet[i] == '\n') {
                pos[0] = 0;
                pos = buf;
                //Print buffer contents
                printf("Msg: %s\n", pos);
                memset(buf,0,128);
                break;
            } else {
                *pos++ = packet[i];
            }
        }
    }
    return 0;
}
