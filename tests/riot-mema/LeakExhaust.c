#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_NAME "RIOT_MEMA"
#define TEST_NAME "LeakExhaust"
#define NUM_BLOCKS 32

static uint8_t pool_data[NUM_BLOCKS * 128];
static memarray_t pool;

int main(void) {
  memarray_init(&pool, pool_data, 128, NUM_BLOCKS);

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

  void *arr[NUM_BLOCKS + 1];

  while (1) {
    uint32_t t1 = ztimer_now(ZTIMER_USEC);
    void *p = memarray_alloc(&pool);
    uint32_t t2 = ztimer_now(ZTIMER_USEC);

    if (!p) {
      printf("EXHAUSTED,alloc_cnt=%lu,free_cnt=%lu\r\n", alloc_cnt, free_cnt);
      fflush(stdout);
      break;
    }
    arr[alloc_cnt] = p;
    alloc_cnt++;
    printf("TIME,leak,alloc,%u,%u,%u,OK,%lu,%lu\r\n", 128U, (unsigned)t1,
           (unsigned)t2, alloc_cnt, free_cnt);
    fflush(stdout);

    size_t free_cur = memarray_available(&pool);
    size_t used_cur = NUM_BLOCKS - free_cur;
    printf("SNAP,after_alloc_%lu,%u,%u\r\n", alloc_cnt, (unsigned)free_cur,
           (unsigned)used_cur);
    fflush(stdout);
  }

  for (uint32_t i = 0; i < alloc_cnt; i++) {
    memarray_free(&pool, arr[i]);
    free_cnt++;
  }

  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
