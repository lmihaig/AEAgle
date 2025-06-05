#include "contiki.h"
#include "lib/heapmem.h"
#include "sys/cc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "DoubleFree"
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

static uint32_t alloc_cnt = 0, free_cnt = 0;

static unsigned long max_observed_allocated_bytes_heapmem = 0;
static void emit_snapshot_contiki_heapmem(const char *phase)
{
  heapmem_stats_t stats;
  heapmem_stats(&stats);

  if (stats.allocated > max_observed_allocated_bytes_heapmem)
  {
    max_observed_allocated_bytes_heapmem = stats.allocated;
  }
  LOG_SNAP_CONTIKI(phase, stats.available, stats.allocated, max_observed_allocated_bytes_heapmem);
}

PROCESS(double_free_test, "Double Free Test");
AUTOSTART_PROCESSES(&double_free_test);

PROCESS_THREAD(double_free_test, ev, data)
{
  static void *p = NULL;
  static clock_time_t tin, tout;

  PROCESS_BEGIN();

  alloc_cnt = 0;
  free_cnt = 0;
  max_observed_allocated_bytes_heapmem = 0;

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_CONTIKI(CLOCK_SECOND);

  emit_snapshot_contiki_heapmem("baseline");

  tin = clock_time();
  p = heapmem_alloc(BLOCK_SIZE);
  tout = clock_time();

  if (p == NULL)
  {
    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_CONTIKI(clock_time(), "OOM");
    goto done_label;
  }
  alloc_cnt++;

  LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
  emit_snapshot_contiki_heapmem("after_setup");

  tin = clock_time();
  heapmem_free(p);
  tout = clock_time();
  free_cnt++;

  LOG_TIME_CONTIKI("setup", "free", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
  emit_snapshot_contiki_heapmem("after_first_free");

  tin = clock_time();
  heapmem_free(p);
  tout = clock_time();
  LOG_TIME_CONTIKI("df_trigger", "free", BLOCK_SIZE, tin, tout, "DF_ATTEMPT", alloc_cnt, free_cnt);
  emit_snapshot_contiki_heapmem("post_primitive_trigger");

done_label:
  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}