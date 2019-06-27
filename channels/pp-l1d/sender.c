#include <stdint.h>
#include <immintrin.h>
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
#define L1_NUM_SETS 64
#define L1_WAYS_PER_SET 8
#define PTE_SIZE 4096
#define SCALE 1
//char data_base __attribute__ ((aligned(4096)))
char data_base[PTE_SIZE*(L1_NUM_SETS*L1_WAYS_PER_SET)] __attribute__((aligned(4096)));
__m256i way_set_indices;
static int random_val;
static volatile char __attribute__((aligned(32768))) buffer[4096*8];
void init(int argc, char const * argv[]) {
    int way_index[8];
    random_val = argc;
    for(int i =0; i < L1_WAYS_PER_SET; i++)
        way_index[i] = i * PTE_SIZE;
    way_set_indices = _mm256_set_epi32 (way_index[0], way_index[1], way_index[2], way_index[3], way_index[4], way_index[5], way_index[6], way_index[7]);
}

void transmit(char * p, int repeat_count)
{
    int sets[64];
    for(int r = 0; r < repeat_count; r++)
    {
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
    /*
    sleep(1);
    printf("Sigaled Sets: ");
    for(int i = 0; i < 64; i++)
        if(sets[i] == 1) printf("-%d",i);
    sleep(1);
    printf("\n");*/
}
void transmit_avx(char * msg, int repeat_count) {
    int x = 1;
    __m256i set_data = _mm256_set_epi32(x^random_val,x^random_val,x^random_val,x^random_val,x^random_val,x^random_val,x^random_val,x^random_val);

    for(int r = 0; r < repeat_count; r++) {
    for(int set_base = 0; set_base < 64; set_base+=32)
    {
        int msg_msk=0;
        for(int i = 0; i < 32; i++) {
            msg_msk |= ((int*)msg)[0] & (1<<(set_base+i));
        }
        __m256i msg_msk_vec= _mm256_set_epi32(msg_msk,msg_msk,msg_msk,msg_msk,msg_msk,msg_msk,msg_msk,msg_msk);

        int s = 0;
        for( ; s < 32; s++) {
            if(((int*)msg)[0] & (1<<(s+set_base))) {
                void * set_data_ptr = (void*)(data_base + 8*(s+set_base));
                set_data= _mm256_mask_i32gather_epi32 (set_data, set_data_ptr, way_set_indices, msg_msk_vec, SCALE);
            }
            msg_msk_vec    = _mm256_slli_epi32(msg_msk_vec, 1);
        }
    }
    }
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
    init(argc,argv);
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
                
                transmit_avx(packet, repeat_count);
                printf("packet sent!");
                sched_yield();
            }
        }
    }
}
