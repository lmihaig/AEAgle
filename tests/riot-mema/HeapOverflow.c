#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "riot-mema"
#define TEST_NAME "HeapOverflow"
#define NUM_BLOCKS 32
#define BLOCK_SIZE 64

static uint8_t pool_data[NUM_BLOCKS * BLOCK_SIZE];
static memarray_t pool;

int main(void)
{
       memarray_init(&pool, pool_data, BLOCK_SIZE, NUM_BLOCKS);

       printf("\r\n");
       printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
       fflush(stdout);

       uint32_t alloc_cnt = 0;
       uint32_t free_cnt = 0;

       size_t free_before = memarray_available(&pool);
       size_t used_before = NUM_BLOCKS - free_before;
       printf("SNAP,baseline,%u,%u\r\n", (unsigned)free_before,
              (unsigned)used_before);
       fflush(stdout);

       uint32_t t1 = ztimer_now(ZTIMER_USEC);
       void *A = memarray_alloc(&pool);
       uint32_t t2 = ztimer_now(ZTIMER_USEC);
       if (A)
       {
              alloc_cnt++;
              printf("TIME,overflow,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
       }
       else
       {
              printf("TIME,overflow,alloc,%u,%u,%u,NULL,%lu,%lu\r\n",
                     (unsigned)BLOCK_SIZE, (unsigned)t1, (unsigned)t2, alloc_cnt,
                     free_cnt);
              printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }
       fflush(stdout);

       size_t free_after_A = memarray_available(&pool);
       size_t used_after_A = NUM_BLOCKS - free_after_A;
       printf("SNAP,after_alloc_A,%u,%u\r\n", (unsigned)free_after_A,
              (unsigned)used_after_A);
       fflush(stdout);

       uint32_t t3 = ztimer_now(ZTIMER_USEC);
       void *B = memarray_alloc(&pool);
       uint32_t t4 = ztimer_now(ZTIMER_USEC);
       if (B)
       {
              alloc_cnt++;
              printf("TIME,overflow,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
       }
       else
       {
              printf("TIME,overflow,alloc,%u,%u,%u,NULL,%lu,%lu\r\n",
                     (unsigned)BLOCK_SIZE, (unsigned)t3, (unsigned)t4, alloc_cnt,
                     free_cnt);
              memarray_free(&pool, A);
              printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }
       fflush(stdout);

       size_t free_after_B = memarray_available(&pool);
       size_t used_after_B = NUM_BLOCKS - free_after_B;
       printf("SNAP,after_alloc_B,%u,%u\r\n", (unsigned)free_after_B,
              (unsigned)used_after_B);
       fflush(stdout);

       size_t corrupt_size = BLOCK_SIZE + sizeof(void *);
       uint32_t t5 = ztimer_now(ZTIMER_USEC);
       memset(A, 0xFF, corrupt_size);
       uint32_t t6 = ztimer_now(ZTIMER_USEC);
       printf("TIME,overflow,write,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)corrupt_size,
              (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t free_after_write = memarray_available(&pool);
       size_t used_after_write = NUM_BLOCKS - free_after_write;
       printf("SNAP,after_write,%u,%u\r\n", (unsigned)free_after_write,
              (unsigned)used_after_write);
       fflush(stdout);

       uint32_t t7 = ztimer_now(ZTIMER_USEC);
       memarray_free(&pool, B);
       uint32_t t8 = ztimer_now(ZTIMER_USEC);
       printf("TIME,overflow,free,%u,%u,%u,BAD_FREE,%lu,%lu\r\n",
              (unsigned)BLOCK_SIZE, (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t free_after_bad = memarray_available(&pool);
       size_t used_after_bad = NUM_BLOCKS - free_after_bad;
       printf("SNAP,after_badfree,%u,%u\r\n", (unsigned)free_after_bad,
              (unsigned)used_after_bad);
       fflush(stdout);

       uint32_t t9 = ztimer_now(ZTIMER_USEC);
       memarray_free(&pool, A);
       uint32_t t10 = ztimer_now(ZTIMER_USEC);
       free_cnt++;
       printf("TIME,cleanup,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
              (unsigned)t9, (unsigned)t10, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t free_after_cleanup = memarray_available(&pool);
       size_t used_after_cleanup = NUM_BLOCKS - free_after_cleanup;
       printf("SNAP,after_cleanup,%u,%u\r\n", (unsigned)free_after_cleanup,
              (unsigned)used_after_cleanup);
       fflush(stdout);

       printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
       return 0;
}
