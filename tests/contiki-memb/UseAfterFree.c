#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "UseAfterFree"
#define BLOCK_SIZE 128
#define BLOCK_COUNT 2
#define PATTERN 0x5A

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

PROCESS(use_after_free_test, "Use After Free Test");
AUTOSTART_PROCESSES(&use_after_free_test);

PROCESS_THREAD(use_after_free_test, ev, data)
{
  static struct block *p1, *p2;
  static clock_time_t tin, tout;
  static uint8_t *buf1, *buf2;
  static int i;
  static int leaked;

  PROCESS_BEGIN();

  printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  memb_init(&test_mem);

  // Allocate first block
  tin = clock_time();
  p1 = memb_alloc(&test_mem);
  tout = clock_time();

  if (p1 == NULL)
  {
    P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "OK");

  // Fill with pattern
  memset(p1->data, PATTERN, BLOCK_SIZE);
  buf1 = p1->data;

  // Free the block
  tin = clock_time();
  int res1 = memb_free(&test_mem, p1);
  tout = clock_time();
  free_cnt++;
  P_TIME("free1", "free", BLOCK_SIZE, tin, tout, res1 == 0 ? "OK" : "ERR");

  // Overwrite after free
  tin = clock_time();
  memset(buf1, 0xA5, BLOCK_SIZE); // overwrite
  tout = clock_time();
  P_TIME("uaf", "memset", BLOCK_SIZE, tin, tout, "DONE");

  // Allocate second block
  tin = clock_time();
  p2 = memb_alloc(&test_mem);
  tout = clock_time();

  if (p2 == NULL)
  {
    P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "OK");

  buf2 = p2->data;

  // Check for leaked pattern
  leaked = 0;
  for (i = 0; i < BLOCK_SIZE; ++i)
  {
    if (buf2[i] == 0xA5)
    {
      leaked = 1;
      break;
    }
  }

  P_TIME("inspect", "check", BLOCK_SIZE, clock_time(), clock_time(),
         leaked ? "LEAKED" : "CLEAN");

  // Cleanup
  tin = clock_time();
  int res2 = memb_free(&test_mem, p2);
  tout = clock_time();
  free_cnt++;
  P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, res2 == 0 ? "OK" : "ERR");

done:
  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
