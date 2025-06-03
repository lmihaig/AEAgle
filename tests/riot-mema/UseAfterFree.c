#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "RIOT_MEMA"
#define TEST_NAME "UseAfterFree"
#define NUM_BLOCKS 32
#define BLOCK_SIZE 64

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
    printf("TIME,uaf,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
           (unsigned)t1, (unsigned)t2, alloc_cnt, free_cnt);
  } else {
    printf("TIME,uaf,alloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
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
  memarray_free(&pool, ptr);
  uint32_t t4 = ztimer_now(ZTIMER_USEC);
  free_cnt++;
  printf("TIME,uaf,free,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
         (unsigned)t3, (unsigned)t4, alloc_cnt, free_cnt);
  fflush(stdout);

  size_t free_after_free = memarray_available(&pool);
  size_t used_after_free = NUM_BLOCKS - free_after_free;
  printf("SNAP,after_free,%u,%u\r\n", (unsigned)free_after_free,
         (unsigned)used_after_free);
  fflush(stdout);

  uint32_t t5 = ztimer_now(ZTIMER_USEC);
  memset(ptr, 0xAA, BLOCK_SIZE);
  uint32_t t6 = ztimer_now(ZTIMER_USEC);
  printf("TIME,uaf,write,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
         (unsigned)t5, (unsigned)t6, alloc_cnt, free_cnt);
  fflush(stdout);

  size_t free_after_write = memarray_available(&pool);
  size_t used_after_write = NUM_BLOCKS - free_after_write;
  printf("SNAP,after_uaf_write,%u,%u\r\n", (unsigned)free_after_write,
         (unsigned)used_after_write);
  fflush(stdout);

  uint32_t t7 = ztimer_now(ZTIMER_USEC);
  void *newptr = memarray_alloc(&pool);
  uint32_t t8 = ztimer_now(ZTIMER_USEC);
  if (newptr) {
    alloc_cnt++;
    printf("TIME,uaf,alloc,%u,%u,%u,OK,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
           (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
  } else {
    printf("TIME,uaf,alloc,%u,%u,%u,NULL,%lu,%lu\r\n", (unsigned)BLOCK_SIZE,
           (unsigned)t7, (unsigned)t8, alloc_cnt, free_cnt);
    printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
    return 0;
  }
  fflush(stdout);

  if (newptr == ptr) {
    printf("LEAK,%p\r\n", ptr);
  } else {
    printf("NOLEAK,%p\r\n", newptr);
  }
  fflush(stdout);

  size_t free_after_realloc = memarray_available(&pool);
  size_t used_after_realloc = NUM_BLOCKS - free_after_realloc;
  printf("SNAP,after_realloc,%u,%u\r\n", (unsigned)free_after_realloc,
         (unsigned)used_after_realloc);
  fflush(stdout);

  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
