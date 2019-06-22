#include <stdint.h>
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
#include <stdio.h>
#include "l1.h"
#include "low.h"
#include "crc16.h"
#define SPAM_COUNT (1000*1000*10)
static volatile char __attribute__((aligned(32768))) buffer[4096*8];

void transmit(char * p)
{
    int sets[64];
    for(int i = 0; i < 64; i++)
        sets[i] = -1;
    for(int c = 0; c < 8; c++){
        for(int b = 0; b < 8; b++) {
            if((p[c] & (1<<b)) == 0)
                continue;
            //displace all ways in set corresponding to bit
            int index = c*8 + b;
            sets[index] = 1;
            for(int w = 0; w < 8; w++)
                buffer[index*64 + 4096*w] = p[c] ^ p[0];
        }
    }
}

int main(int argc, char **argv)
{
    uint32_t msg_num = 0;
    setbuf(stdout, NULL);

    printf("Select a set to toggle. (type start to begin contention generation)\n");
    uint64_t sets = 0;
    char packet[8] = {0,0,0,0,0,0,0,0};
    if(argc <= 1) {
        printf("Usage: pass L1d sets to activate\nEg: contend on sets 0 1 2 and 3 by using isca-cc-create-l1d-contention 0 1 2 3\n");
        return 1;
    }
    for(int i = 1; i < argc; i++){
        int d = atoi(argv[i]);
        if(d > 0 && d < 64)
        {
            uint64_t set_num = 1<<d;
            sets ^= set_num;
            if(sets & set_num)
                printf("enabled set %d\n",d);
            else
                printf("disabled set %d\n",d);
        }

    }
    uint64_t enabled_sets = sets;
    for(int i = 0; i < 8; i++) {
        char c = 0;
        for(int j = 0; j < 8; j++) {
            if(enabled_sets & 1)
                c |= (1<<j);
            enabled_sets>>1;
        }
        packet[i] = c;
    }
    while(1) 
    {
        for(int r = 0; r < SPAM_COUNT; r++) {
            transmit(packet);
        }
        asm("pause");
    }
}
