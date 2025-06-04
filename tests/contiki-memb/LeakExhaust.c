#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "LeakExhaust"
#define BLOCK_SIZE 128
#define BLOCK_COUNT 256

struct block
{
  uint8_t data[BLOCK_SIZE];
};

MEMB(test_mem, struct block, BLOCK_COUNT);

static uint32_t alloc_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\n", CLOCK_SECOND)

#define P_TIME(ph, op, sz, ti, to, res)                                 \
  printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%u\r\n", ph, op, (unsigned)(sz), \
         (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, 0)

#define P_FAULT(res) \
  printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(leak_exhaust_test, "Leak Exhaust Test");
AUTOSTART_PROCESSES(&leak_exhaust_test);

PROCESS_THREAD(leak_exhaust_test, ev, data)
{
  static struct block *p;
  static clock_time_t tin, tout;

  PROCESS_BEGIN();

  printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  memb_init(&test_mem);

  while (1)
  {
    tin = clock_time();
    p = memb_alloc(&test_mem);
    tout = clock_time();

    if (p == NULL)
    {
      P_TIME("leak", "malloc", BLOCK_SIZE, tin, tout, "NULL");
      P_FAULT("OOM");
      break;
    }
    alloc_cnt++;
    P_TIME("leak", "malloc", BLOCK_SIZE, tin, tout, "OK");
  }

  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
