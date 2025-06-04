#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "FakeFree"
#define BLOCK_SIZE 128
#define BLOCK_COUNT 1

struct block
{
  uint8_t data[BLOCK_SIZE];
};

MEMB(test_mem, struct block, BLOCK_COUNT);

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\n", CLOCK_SECOND)

#define P_TIME(ph, op, sz, ti, to, res)                                  \
  printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n", ph, op, (unsigned)(sz), \
         (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)

#define P_FAULT(res) \
  printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(fake_free_test, "Fake Free Test");
AUTOSTART_PROCESSES(&fake_free_test);

PROCESS_THREAD(fake_free_test, ev, data)
{
  static struct block *p;
  static void *p_offset;
  static clock_time_t tin, tout;
  const size_t OFFSET = BLOCK_SIZE / 2;

  PROCESS_BEGIN();

  printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  memb_init(&test_mem);

  // Allocate memory
  tin = clock_time();
  p = memb_alloc(&test_mem);
  tout = clock_time();

  if (p == NULL)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");

  // Attempt to free an offset pointer (invalid free)
  p_offset = (void *)((uint8_t *)p + OFFSET);
  tin = clock_time();
  int res1 = memb_free(&test_mem, p_offset);
  tout = clock_time();

  free_cnt++;
  P_TIME("fakefree", "free", (uint32_t)OFFSET, tin, tout,
         res1 == 0 ? "OK" : "BAD_FREE");

  // Properly free the original pointer
  tin = clock_time();
  int res2 = memb_free(&test_mem, p);
  tout = clock_time();

  free_cnt++;
  P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, res2 == 0 ? "OK" : "ERR");

done:
  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
