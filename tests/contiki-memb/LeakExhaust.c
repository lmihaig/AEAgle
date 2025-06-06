#include "contiki.h"
#include "sys/rtimer.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "LeakExhaust"
#define BLOCK_COUNT 256
#define BLOCK_SIZE 128

#define PRINTF_LOG_CONTIKI(format, ...) printf(format, ##__VA_ARGS__)

#define LOG_TEST_START(alloc_name, test_name_str) \
  PRINTF_LOG_CONTIKI("# %s %s start\r\n", (alloc_name), (test_name_str))

#define LOG_TEST_END(alloc_name, test_name_str) \
  PRINTF_LOG_CONTIKI("# %s %s end\r\n", (alloc_name), (test_name_str))

#define LOG_META_CONTIKI(tick_hz_val) \
  PRINTF_LOG_CONTIKI("META,tick_hz,%u\r\n", (unsigned)(tick_hz_val))

#define LOG_TIME_CONTIKI(phase_str, op_str, size_val, time_in, time_out, result_str, ac, fc) \
  PRINTF_LOG_CONTIKI("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n",                                 \
                     (phase_str), (op_str), (unsigned)(size_val),                            \
                     (unsigned long)(time_in), (unsigned long)(time_out), (result_str),      \
                     (unsigned long)(ac), (unsigned long)(fc))

#define LOG_SNAP_CONTIKI(phase_str, free_b_val, allocated_b_val, max_alloc_b_val) \
  PRINTF_LOG_CONTIKI("SNAP,%s,%lu,%lu,%lu\r\n",                                   \
                     (phase_str), (unsigned long)(free_b_val),                    \
                     (unsigned long)(allocated_b_val), (unsigned long)(max_alloc_b_val))

#define LOG_FAULT_CONTIKI(current_ticks, error_str) \
  PRINTF_LOG_CONTIKI("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)(current_ticks), (error_str))

struct block
{
  uint8_t data[BLOCK_SIZE];
};

MEMB(test_mem, struct block, BLOCK_COUNT);

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;
static unsigned long max_allocated_bytes_contiki_memb = 0;

static void emit_snapshot_contiki_memb(const char *phase)
{
  unsigned int free_blocks = memb_numfree(&test_mem);
  unsigned int used_blocks = BLOCK_COUNT - free_blocks;
  unsigned long current_allocated_bytes = (unsigned long)used_blocks * BLOCK_SIZE;
  unsigned long current_free_bytes = (unsigned long)free_blocks * BLOCK_SIZE;

  if (current_allocated_bytes > max_allocated_bytes_contiki_memb)
  {
    max_allocated_bytes_contiki_memb = current_allocated_bytes;
  }
  LOG_SNAP_CONTIKI(phase, current_free_bytes, current_allocated_bytes, max_allocated_bytes_contiki_memb);
}

PROCESS(leak_exhaust_test, "Leak Exhaust Test");
AUTOSTART_PROCESSES(&leak_exhaust_test);

PROCESS_THREAD(leak_exhaust_test, ev, data)
{
  static struct block *p;
  static rtimer_clock_t tin, tout;

  PROCESS_BEGIN();

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);

  LOG_META_CONTIKI(RTIMER_SECOND);

  memb_init(&test_mem);
  emit_snapshot_contiki_memb("baseline");

  while (1)
  {
    tin = RTIMER_NOW();
    p = memb_alloc(&test_mem);
    tout = RTIMER_NOW();

    if (p == NULL)
    {
      LOG_TIME_CONTIKI("leakloop", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
      LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
      break;
    }
    alloc_cnt++;
    LOG_TIME_CONTIKI("leakloop", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    emit_snapshot_contiki_memb("after_alloc");
  }

  emit_snapshot_contiki_memb("after_leakloop_exhaustion");

  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}