#include "malloc_monitor.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_NAME "riot-tlsf"
#define TEST_NAME "MixedLifetime"
#define HEAP_SIZE 65536
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

int main(void)
{
       malloc_monitor_reset_high_watermark();

       printf("\r\n");
       printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
       fflush(stdout);

       void *pinned[PIN_COUNT];
       void *buf[BURST_COUNT];
       uint32_t alloc_cnt = 0;
       uint32_t free_cnt = 0;

       size_t before_cur = malloc_monitor_get_usage_current();
       size_t before_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,baseline,%u,%u\r\n", (unsigned)before_cur,
              (unsigned)before_high);
       fflush(stdout);

       for (int i = 0; i < PIN_COUNT; ++i)
       {
              uint32_t t1 = ztimer_now(ZTIMER_USEC);
              pinned[i] = malloc(BLOCK_SIZE * 2);
              uint32_t t2 = ztimer_now(ZTIMER_USEC);
              if (!pinned[i])
              {
                     printf("TIME,pin,malloc,%u,%u,%u,NULL,%lu,%lu\r\n",
                            (unsigned)(BLOCK_SIZE * 2), (unsigned)t1, (unsigned)t2, alloc_cnt,
                            free_cnt);
                     printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
                     return 0;
              }
              alloc_cnt++;
              printf("TIME,pin,malloc,%u,%u,%u,OK,%lu,%lu\r\n",
                     (unsigned)(BLOCK_SIZE * 2), (unsigned)t1, (unsigned)t2, alloc_cnt,
                     free_cnt);
              fflush(stdout);

              size_t after_pin_cur = malloc_monitor_get_usage_current();
              size_t after_pin_high = malloc_monitor_get_usage_high_watermark();
              printf("SNAP,after_pin_%d,%u,%u\r\n", i + 1, (unsigned)after_pin_cur,
                     (unsigned)after_pin_high);
              fflush(stdout);
       }

       for (int round = 1; round <= BURST_ROUNDS; ++round)
       {
              int i;
              for (i = 0; i < BURST_COUNT; ++i)
              {
                     uint32_t t3 = ztimer_now(ZTIMER_USEC);
                     buf[i] = malloc(BLOCK_SIZE);
                     uint32_t t4 = ztimer_now(ZTIMER_USEC);
                     if (!buf[i])
                     {
                            printf("TIME,burst,malloc,%u,%u,%u,NULL,%lu,%lu\r\n",
                                   (unsigned)BLOCK_SIZE, (unsigned)t3, (unsigned)t4, alloc_cnt,
                                   free_cnt);
                            printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
                            goto cleanup;
                     }
                     alloc_cnt++;
                     printf("TIME,burst,malloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                            (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
                     fflush(stdout);
              }

              size_t after_burst_alloc_cur = malloc_monitor_get_usage_current();
              size_t after_burst_alloc_high = malloc_monitor_get_usage_high_watermark();
              printf("SNAP,after_burst_alloc_%02d,%u,%u\r\n", round,
                     (unsigned)after_burst_alloc_cur, (unsigned)after_burst_alloc_high);
              fflush(stdout);

              for (int j = i - 1; j >= 0; --j)
              {
                     uint32_t t5 = ztimer_now(ZTIMER_USEC);
                     free(buf[j]);
                     uint32_t t6 = ztimer_now(ZTIMER_USEC);
                     free_cnt++;
                     printf("TIME,burst,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                            (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
                     fflush(stdout);

                     size_t after_burst_free_cur = malloc_monitor_get_usage_current();
                     size_t after_burst_free_high = malloc_monitor_get_usage_high_watermark();
                     printf("SNAP,after_burst_free_%02d_%02d,%u,%u\r\n", round, j + 1,
                            (unsigned)after_burst_free_cur, (unsigned)after_burst_free_high);
                     fflush(stdout);
              }
       }

cleanup:
       for (int i = 0; i < PIN_COUNT; ++i)
       {
              uint32_t t7 = ztimer_now(ZTIMER_USEC);
              free(pinned[i]);
              uint32_t t8 = ztimer_now(ZTIMER_USEC);
              free_cnt++;
              printf("TIME,cleanup,free,%u,%u,%u,OK,%lu,%lu\r\n",
                     (unsigned)(BLOCK_SIZE * 2), (unsigned)t7, (unsigned)t8, alloc_cnt,
                     free_cnt);
              fflush(stdout);

              size_t after_cleanup_cur = malloc_monitor_get_usage_current();
              size_t after_cleanup_high = malloc_monitor_get_usage_high_watermark();
              printf("SNAP,after_cleanup_%d,%u,%u\r\n", i + 1,
                     (unsigned)after_cleanup_cur, (unsigned)after_cleanup_high);
              fflush(stdout);
       }

       printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
       return 0;
}
