#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "RIOT_MEMA"
#define TEST_NAME "FakeFree"
#define NUM_BLOCKS 32
#define BLOCK_SIZE 128
#define OFFSET 16

static uint8_t pool_data[NUM_BLOCKS * BLOCK_SIZE];
static memarray_t pool;

int main(void) {
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
  void *ptr = memarray_alloc(&pool);
  uint32_t t2 = ztimer_now(ZTIMER_USEC);
  if (ptr) {
    alloc_cnt++;
    printf("TIME,setup,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
           (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
  } else {
    printf("TIME,setup,alloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
           (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
    printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
    return 0;
  }
  fflush(stdout);

  size_t free_after_alloc = memarray_available(&pool);
  size_t used_after_alloc = NUM_BLOCKS - free_after_alloc;
  printf("SNAP,after_alloc,%u,%u\r\n", (unsigned)free_after_alloc,
         (unsigned)used_after_alloc);
  fflush(stdout);

  uint32_t t3 = ztimer_now(ZTIMER_USEC);
  memarray_free(&pool, (uint8_t *)ptr + OFFSET);
  uint32_t t4 = ztimer_now(ZTIMER_USEC);
  printf("TIME,fakefree,free,%u,%u,%u,BAD_FREE,%lu,%lu\r\n", (unsigned)OFFSET,
         (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
  fflush(stdout);

  size_t free_after_bad = memarray_available(&pool);
  size_t used_after_bad = NUM_BLOCKS - free_after_bad;
  printf("SNAP,after_fakefree,%u,%u\r\n", (unsigned)free_after_bad,
         (unsigned)used_after_bad);
  fflush(stdout);

  uint32_t t5 = ztimer_now(ZTIMER_USEC);
  memarray_free(&pool, ptr);
  uint32_t t6 = ztimer_now(ZTIMER_USEC);
  free_cnt++;
  printf("TIME,cleanup,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
         (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
  fflush(stdout);

  size_t free_after_cleanup = memarray_available(&pool);
  size_t used_after_cleanup = NUM_BLOCKS - free_after_cleanup;
  printf("SNAP,after_cleanup,%u,%u\r\n", (unsigned)free_after_cleanup,
         (unsigned)used_after_cleanup);
  fflush(stdout);

  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
