#include "contiki.h"
#include "lib/heapmem.h"
#include "sys/cc.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "DoubleFree"
#define BLOCK_SIZE 128

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\n", CLOCK_SECOND)
#define P_TIME(ph, op, sz, ti, to, res)                                  \
  printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n", ph, op, (unsigned)(sz), \
         (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)
#define P_FAULT(res) \
  printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(double_free_test, "Double Free Test");
AUTOSTART_PROCESSES(&double_free_test);

PROCESS_THREAD(double_free_test, ev, data)
{
  static void *p;
  static clock_time_t tin, tout;

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

  tin = clock_time();
  heapmem_free(p);
  tout = clock_time();
  free_cnt++;
  P_TIME("free1", "free", BLOCK_SIZE, tin, tout, "OK");

  tin = clock_time();
  heapmem_free(p); // Double free
  tout = clock_time();
  free_cnt++;
  P_TIME("free2", "free", BLOCK_SIZE, tin, tout, "dblfree");

done:
  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
