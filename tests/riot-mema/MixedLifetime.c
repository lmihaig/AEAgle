#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_NAME "riot-mema"
#define TEST_NAME "MixedLifetime"
#define NUM_BLOCKS 64
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

static uint8_t pool_data[NUM_BLOCKS * BLOCK_SIZE];
static memarray_t pool;

int main(void)
{
       memarray_init(&pool, pool_data, BLOCK_SIZE, NUM_BLOCKS);

       printf("\r\n");
       printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
       fflush(stdout);

       void *pinned[PIN_COUNT];
       void *buf[BURST_COUNT];
       uint32_t alloc_cnt = 0;
       uint32_t free_cnt = 0;

       size_t free_before = memarray_available(&pool);
       size_t used_before = NUM_BLOCKS - free_before;
       printf("SNAP,baseline,%u,%u\r\n", (unsigned)free_before,
              (unsigned)used_before);
       fflush(stdout);

       for (int i = 0; i < PIN_COUNT; ++i)
       {
              uint32_t t1 = ztimer_now(ZTIMER_USEC);
              pinned[i] = memarray_alloc(&pool);
              uint32_t t2 = ztimer_now(ZTIMER_USEC);
              if (!pinned[i])
              {
                     printf("TIME,pin,alloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                            (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
                     printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
                     return 0;
              }
              alloc_cnt++;
              printf("TIME,pin,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
              fflush(stdout);

              size_t free_cur = memarray_available(&pool);
              size_t used_cur = NUM_BLOCKS - free_cur;
              printf("SNAP,after_pin_%d,%u,%u\r\n", i + 1, (unsigned)free_cur,
                     (unsigned)used_cur);
              fflush(stdout);
       }

       for (int round = 1; round <= BURST_ROUNDS; ++round)
       {
              int i;
              for (i = 0; i < BURST_COUNT; ++i)
              {
                     uint32_t t3 = ztimer_now(ZTIMER_USEC);
                     buf[i] = memarray_alloc(&pool);
                     uint32_t t4 = ztimer_now(ZTIMER_USEC);
                     if (!buf[i])
                     {
                            printf("TIME,burst,alloc,%u,%u,%u,NULL,%lu,%lu\r\n",
                                   (unsigned)BLOCK_SIZE, (unsigned)t3, (unsigned)t4, alloc_cnt,
                                   free_cnt);
                            printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
                            goto cleanup;
                     }
                     alloc_cnt++;
                     printf("TIME,burst,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                            (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
                     fflush(stdout);

                     size_t free_cur = memarray_available(&pool);
                     size_t used_cur = NUM_BLOCKS - free_cur;
                     printf("SNAP,after_burst_alloc_%02d_%02d,%u,%u\r\n", round, i + 1,
                            (unsigned)free_cur, (unsigned)used_cur);
                     fflush(stdout);
              }

              for (int j = i - 1; j >= 0; --j)
              {
                     uint32_t t5 = ztimer_now(ZTIMER_USEC);
                     memarray_free(&pool, buf[j]);
                     uint32_t t6 = ztimer_now(ZTIMER_USEC);
                     free_cnt++;
                     printf("TIME,burst,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                            (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
                     fflush(stdout);

                     size_t free_cur = memarray_available(&pool);
                     size_t used_cur = NUM_BLOCKS - free_cur;
                     printf("SNAP,after_burst_free_%02d_%02d,%u,%u\r\n", round, j + 1,
                            (unsigned)free_cur, (unsigned)used_cur);
                     fflush(stdout);
              }
       }

cleanup:
       for (int i = 0; i < PIN_COUNT; ++i)
       {
              uint32_t t7 = ztimer_now(ZTIMER_USEC);
              memarray_free(&pool, pinned[i]);
              uint32_t t8 = ztimer_now(ZTIMER_USEC);
              free_cnt++;
              printf("TIME,cleanup,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
              fflush(stdout);

              size_t free_cur = memarray_available(&pool);
              size_t used_cur = NUM_BLOCKS - free_cur;
              printf("SNAP,after_cleanup_%d,%u,%u\r\n", i + 1, (unsigned)free_cur,
                     (unsigned)used_cur);
              fflush(stdout);
       }

       printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
       return 0;
}
