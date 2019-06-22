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
#define SPAM_COUNT 1250
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
    /*
    sleep(1);
    printf("Sigaled Sets: ");
    for(int i = 0; i < 64; i++)
        if(sets[i] == 1) printf("-%d",i);
    sleep(1);
    printf("\n");*/
}

uint32_t finalize_packet(char * p, uint32_t packet_num)
{
    unsigned char num_u = (unsigned char)(packet_num >> 8);  
    unsigned char num_l = (unsigned char)(packet_num & 0xff);  
    p[0] = num_l;
    p[1] = num_u;
    uint16_t crc = crc16_ccitt_init();
    for(int i = 0; i < 6; i++)
        crc = crc16_ccitt_update(p[i], crc);
    crc = crc16_ccitt_finalize(crc);
    unsigned crc_l = (unsigned char) (crc & 0xff);
    unsigned crc_u = (unsigned char) (crc >> 8);
    //store in big-endian order that crc lib expects
    p[6] = crc_u;
    p[7] = crc_l;
    uint32_t next_packet = packet_num++;
    return (next_packet & 0xffff);
}

int main(int argc, char **argv)
{
    uint32_t msg_num = 0;
    setbuf(stdout, NULL);

    int repeat_count = SPAM_COUNT;
    printf("Please type a message (exit to stop).");
    if(argc > 1) {
        repeat_count = atoi(argv[1]);
        if(repeat_count < 0) {
            printf("please use positive repeat count\n");
            return 1;
        }
        printf(" Repeat count set to %d", repeat_count);
    }
    printf("\n");


    while (1) {
        // Get a message to send from the user
        printf("\n< ");
        char text_buf[128];
        fgets(text_buf, sizeof(text_buf), stdin);

        if (strcmp(text_buf, "exit\n") == 0) {
            return 0;
        }

        char * pos = text_buf;
        char * last_byte = text_buf + 128;
        int msg_len = strlen(text_buf);

        uint32_t first_msg = msg_num;
        uint32_t num_packets = (msg_len+3)/4;
        while(msg_len > 0)
        {
            if(msg_len > 0) {
                char packet[8] = {0,0,0,0,0,0,0,0};
                int n = 2;
                char * lim = pos + 4;
                if(lim > last_byte)
                    lim = last_byte;
                while(pos < lim && msg_len--) {
                    packet[n++] = *pos;
                    pos++;
                }

                //Add packet number and CRC information to packet
                finalize_packet(packet, msg_num++);
                

                printf("\n------> sending packet %d/%d....",msg_num-first_msg, num_packets);
                for(int r = 0; r < repeat_count; r++) {
                    transmit(packet);
                }
                printf("packet sent!");
                sched_yield();
            }
        }
    }
}
