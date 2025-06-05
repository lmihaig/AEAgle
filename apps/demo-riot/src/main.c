#include <stdio.h>
#include <stdlib.h> // For malloc, free (typically TLSF in RIOT)
#include <string.h> // For snprintf, memset
#include <stdint.h> // For uint8_t

#include "memarray.h" // For memarray allocator

// Defines for TLSF (malloc/free) demonstration
#define TLSF_ALLOC_SIZE 40

// Defines for memarray demonstration
#define MEMA_BLOCK_SIZE 32
#define MEMA_NUM_BLOCKS 2
static uint8_t mema_pool_data[MEMA_NUM_BLOCKS * MEMA_BLOCK_SIZE];
static memarray_t my_mema_pool;

int main(void)
{
       char *tlsf_message_buffer;
       char *mema_message_buffer;

       puts("RIOT OS Memory Demo - Concise\r\n");

       // --- TLSF (malloc/free) Demo ---
       tlsf_message_buffer = (char *)malloc(TLSF_ALLOC_SIZE);

       if (tlsf_message_buffer != NULL)
       {
              memset(tlsf_message_buffer, 0, TLSF_ALLOC_SIZE);
              snprintf(tlsf_message_buffer, TLSF_ALLOC_SIZE, "dynamically allocated memory (TLSF)");
              printf("Hello from %s\r\n", tlsf_message_buffer);
              free(tlsf_message_buffer);
              tlsf_message_buffer = NULL;
       }
       else
       {
              puts("malloc (TLSF) failed.");
       }

       // --- Memarray Demo ---
       memarray_init(&my_mema_pool, mema_pool_data, MEMA_BLOCK_SIZE, MEMA_NUM_BLOCKS);
       mema_message_buffer = (char *)memarray_alloc(&my_mema_pool);

       if (mema_message_buffer != NULL)
       {
              memset(mema_message_buffer, 0, MEMA_BLOCK_SIZE);
              snprintf(mema_message_buffer, MEMA_BLOCK_SIZE, "fixed-block memory (memarray)");
              printf("Hello from %s\r\n", mema_message_buffer);
              memarray_free(&my_mema_pool, mema_message_buffer);
              mema_message_buffer = NULL;
       }
       else
       {
              puts("memarray_alloc failed.");
       }

       puts("\r\nDemo finished.");
       return 0;
}
