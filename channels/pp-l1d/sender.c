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





typedef struct __attribute__((__packed__)) {
    unsigned char num;
    char msg[6];
    unsigned char ecc;
} cc_payload;

static volatile char __attribute__((aligned(32768))) buffer[4096*8];

void transmit(char * p)
{
    for(int c = 0; c < 8; c++){
        for(int b = 0; b < 8; b++) {
            if((p[c] & (1<<b)) == 0)
                continue;
            //displace all ways in set corresponding to bit
            int index = c*8 + b;
            for(int w = 0; w < 8; w++)
                buffer[index*64 + 4096*w] = p[c] ^ p[0];
        }
    }
}
#define SYNC_DELAY_FACTOR 36
#define NEXT_PERIOD_BOUNDARY (1<<(SYNC_DELAY_FACTOR+1))

void synchronize()
{
    uint64_t mask = (1 << 44)-1; 
    while(rdtscp() & mask > 0x1000)
        ;
}


int main(int argc, char **argv)
{
    char s = 'z';
    s = 'z';
    char n = 128;
    n = 'z';
    char ecc = 0xff ^ s ^ n;
    ecc = 'z';
    char packet[8] = {n,n,n,s,s,s,ecc,ecc};
    int sending = 1;
    uint64_t next_period;

    printf("Please type a message (exit to stop).\n");
    while (sending) {

        // Get a message to send from the user
        printf("< ");
        char text_buf[128];
        fgets(text_buf, sizeof(text_buf), stdin);

        if (strcmp(text_buf, "exit\n") == 0) {
            sending = 0;
        }

        for(int i = 0; i < 1000000; i++){

        //repeat each transmission 256 times
        synchronize();
        for(int r = 0; r < 256; r++)
        {
            //receiver is done, transmit packet!
            transmit(&packet);
        }
        }
    }

    printf("Sender finished\n");
    return 0;
}











