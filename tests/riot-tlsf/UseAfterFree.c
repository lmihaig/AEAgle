#include "malloc_monitor.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "riot-tlsf"
#define TEST_NAME "UseAfterFree"
#define HEAP_SIZE 65536
#define BLOCK_SIZE 64

int main(void)
{
       malloc_monitor_reset_high_watermark();

       printf("\r\n");
       printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
       fflush(stdout);

       uint32_t alloc_cnt = 0;
       uint32_t free_cnt = 0;

       size_t before_cur = malloc_monitor_get_usage_current();
       size_t before_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,baseline,%u,%u\r\n", (unsigned)before_cur,
              (unsigned)before_high);
       fflush(stdout);

       uint32_t t1 = ztimer_now(ZTIMER_USEC);
       void *ptr = malloc(BLOCK_SIZE);
       uint32_t t2 = ztimer_now(ZTIMER_USEC);
       if (ptr)
       {
              alloc_cnt++;
              printf("TIME,uaf,malloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
       }
       else
       {
              printf("TIME,uaf,malloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
              printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }
       fflush(stdout);

       size_t after_alloc_cur = malloc_monitor_get_usage_current();
       size_t after_alloc_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,after_alloc,%u,%u\r\n", (unsigned)after_alloc_cur,
              (unsigned)after_alloc_high);
       fflush(stdout);

       uint32_t t3 = ztimer_now(ZTIMER_USEC);
       free(ptr);
       uint32_t t4 = ztimer_now(ZTIMER_USEC);
       free_cnt++;
       printf("TIME,uaf,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
              (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t after_free_cur = malloc_monitor_get_usage_current();
       size_t after_free_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,after_free,%u,%u\r\n", (unsigned)after_free_cur,
              (unsigned)after_free_high);
       fflush(stdout);

       uint32_t t5 = ztimer_now(ZTIMER_USEC);
       memset(ptr, 0xAA, BLOCK_SIZE);
       uint32_t t6 = ztimer_now(ZTIMER_USEC);
       printf("TIME,uaf,write,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
              (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t after_write_cur = malloc_monitor_get_usage_current();
       size_t after_write_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,after_uaf_write,%u,%u\r\n", (unsigned)after_write_cur,
              (unsigned)after_write_high);
       fflush(stdout);

       uint32_t t7 = ztimer_now(ZTIMER_USEC);
       void *newptr = malloc(BLOCK_SIZE);
       uint32_t t8 = ztimer_now(ZTIMER_USEC);
       if (newptr)
       {
              alloc_cnt++;
              printf("TIME,uaf,malloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
       }
       else
       {
              printf("TIME,uaf,malloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
                     (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
              printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }
       fflush(stdout);

       if (newptr == ptr)
       {
              printf("LEAK,%p\r\n", ptr);
       }
       else
       {
              printf("NOLEAK,%p\r\n", newptr);
       }
       fflush(stdout);

       size_t after_realloc_cur = malloc_monitor_get_usage_current();
       size_t after_realloc_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,after_realloc,%u,%u\r\n", (unsigned)after_realloc_cur,
              (unsigned)after_realloc_high);
       fflush(stdout);

       uint32_t t9 = ztimer_now(ZTIMER_USEC);
       free(newptr);
       uint32_t t10 = ztimer_now(ZTIMER_USEC);
       free_cnt++;
       printf("TIME,cleanup,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
              (unsigned)t9, (unsigned)t10, alloc_cnt, free_cnt);
       fflush(stdout);

       size_t after_cleanup_cur = malloc_monitor_get_usage_current();
       size_t after_cleanup_high = malloc_monitor_get_usage_high_watermark();
       printf("SNAP,after_cleanup,%u,%u\r\n", (unsigned)after_cleanup_cur,
              (unsigned)after_cleanup_high);
       fflush(stdout);

       printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
       return 0;
}
