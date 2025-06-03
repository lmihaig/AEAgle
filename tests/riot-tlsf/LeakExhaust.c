#include "malloc_monitor.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_NAME "RIOT_TLSF"
#define TEST_NAME "LeakExhaust"
#define HEAP_SIZE 65536
#define ALLOC_SIZE 128

int main(void) {
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

  while (1) {
    uint32_t t1 = ztimer_now(ZTIMER_USEC);
    void *p = malloc(ALLOC_SIZE);
    uint32_t t2 = ztimer_now(ZTIMER_USEC);

    if (!p) {
      printf("EXHAUSTED,alloc_cnt=%lu,free_cnt=%lu\r\n", alloc_cnt, free_cnt);
      fflush(stdout);
      break;
    }
    alloc_cnt++;
    printf("TIME,leak,malloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)ALLOC_SIZE,
           (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
    fflush(stdout);

    size_t cur_cur = malloc_monitor_get_usage_current();
    size_t cur_high = malloc_monitor_get_usage_high_watermark();
    printf("SNAP,after_malloc_%lu,%u,%u\r\n", alloc_cnt, (unsigned)cur_cur,
           (unsigned)cur_high);
    fflush(stdout);
  }

  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
