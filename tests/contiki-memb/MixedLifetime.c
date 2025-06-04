#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10
#define TOTAL_BLOCKS (PIN_COUNT + BURST_ROUNDS * BURST_COUNT)

struct block
{
  uint8_t data[BLOCK_SIZE];
};

MEMB(test_mem, struct block, TOTAL_BLOCKS);

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\n", CLOCK_SECOND)

#define P_TIME(ph, op, sz, ti, to, res)                                  \
  printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n", ph, op, (unsigned)(sz), \
         (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)

#define P_FAULT(res) \
  printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(mixed_lifetime_test, "Mixed Lifetime Test");
AUTOSTART_PROCESSES(&mixed_lifetime_test);

PROCESS_THREAD(mixed_lifetime_test, ev, data)
{
  static struct block *pinned[PIN_COUNT];
  static struct block *buf[BURST_COUNT];
  static clock_time_t tin, tout;
  int i, round;

  PROCESS_BEGIN();

  printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  memb_init(&test_mem);

  for (i = 0; i < PIN_COUNT; ++i)
  {
    tin = clock_time();
    pinned[i] = memb_alloc(&test_mem);
    tout = clock_time();
    if (!pinned[i])
    {
      P_TIME("pin", "malloc", BLOCK_SIZE, tin, tout, "NULL");
      P_FAULT("OOM");
      goto done;
    }
    alloc_cnt++;
    P_TIME("pin", "malloc", BLOCK_SIZE, tin, tout, "OK");
  }

  for (round = 1; round <= BURST_ROUNDS; ++round)
  {
    for (i = 0; i < BURST_COUNT; ++i)
    {
      tin = clock_time();
      buf[i] = memb_alloc(&test_mem);
      tout = clock_time();
      if (!buf[i])
      {
        P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto cleanup;
      }
      alloc_cnt++;
      P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
    }

    for (i = BURST_COUNT - 1; i >= 0; --i)
    {
      tin = clock_time();
      memb_free(&test_mem, buf[i]);
      tout = clock_time();
      free_cnt++;
      P_TIME("burst", "free", BLOCK_SIZE, tin, tout, "OK");
    }
  }

cleanup:
  for (i = 0; i < PIN_COUNT; ++i)
  {
    tin = clock_time();
    memb_free(&test_mem, pinned[i]);
    tout = clock_time();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, "OK");
  }

done:
  printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}
