#include "contiki.h"
#include "lib/heapmem.h"
#include <stdint.h>
#include <stdio.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "FakeFree"
#define BLOCK_SIZE 128

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
  static void *p;
  static uint8_t *p_offset;
  static clock_time_t tin, tout;
  const size_t OFFSET = BLOCK_SIZE / 2;

  PROCESS_BEGIN();

  printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  tin = clock_time();
  p = heapmem_alloc(BLOCK_SIZE);
  tout = clock_time();

  if (p == NULL)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");

  p_offset = (uint8_t *)p + OFFSET;
  tin = clock_time();
  heapmem_free(p_offset);
  tout = clock_time();
  free_cnt++;
  P_TIME("fakefree", "free", (uint32_t)OFFSET, tin, tout, "BAD_FREE");

  tin = clock_time();
  heapmem_free(p);
  tout = clock_time();
  free_cnt++;
  P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, "OK");

done:
  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
