#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10
#define TOTAL_BLOCKS (PIN_COUNT + BURST_COUNT)

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

MEMB(test_mem, struct block, TOTAL_BLOCKS);

static uint32_t alloc_cnt = 0, free_cnt = 0;
static unsigned long max_allocated_bytes_contiki_memb = 0;

static void emit_snapshot_contiki_memb(const char *phase)
{
  unsigned int free_blocks = memb_numfree(&test_mem);
  unsigned int used_blocks = TOTAL_BLOCKS - free_blocks;
  unsigned long current_allocated_bytes = (unsigned long)used_blocks * BLOCK_SIZE;
  unsigned long current_free_bytes = (unsigned long)free_blocks * BLOCK_SIZE;

  if (current_allocated_bytes > max_allocated_bytes_contiki_memb)
  {
    max_allocated_bytes_contiki_memb = current_allocated_bytes;
  }
  LOG_SNAP_CONTIKI(phase, current_free_bytes, current_allocated_bytes, max_allocated_bytes_contiki_memb);
}

PROCESS(mixed_lifetime_test, "Mixed Lifetime Test");
AUTOSTART_PROCESSES(&mixed_lifetime_test);

PROCESS_THREAD(mixed_lifetime_test, ev, data)
{
  static struct block *pinned[PIN_COUNT];
  static struct block *buf[BURST_COUNT];
  static clock_time_t tin, tout, t_cleanup_in, t_cleanup_out;
  static int i, round, j;
  static char snap_phase_label[64];
  static int successfully_pinned = 0;
  static int current_burst_successful_allocs = 0;
  static int res_free;

  PROCESS_BEGIN();

  for (i = 0; i < PIN_COUNT; ++i)
    pinned[i] = NULL;
  for (i = 0; i < BURST_COUNT; ++i)
    buf[i] = NULL;
  alloc_cnt = 0;
  free_cnt = 0;
  max_allocated_bytes_contiki_memb = 0;
  successfully_pinned = 0;

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_CONTIKI(CLOCK_SECOND);

  memb_init(&test_mem);
  emit_snapshot_contiki_memb("baseline");

  for (i = 0; i < PIN_COUNT; ++i)
  {
    tin = clock_time();
    pinned[i] = memb_alloc(&test_mem);
    tout = clock_time();
    if (!pinned[i])
    {
      LOG_TIME_CONTIKI("pin", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
      LOG_FAULT_CONTIKI(clock_time(), "OOM");
      goto cleanup_logic;
    }
    alloc_cnt++;
    LOG_TIME_CONTIKI("pin", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    successfully_pinned++;
  }
  emit_snapshot_contiki_memb("after_pins");

  for (round = 1; round <= BURST_ROUNDS; ++round)
  {
    current_burst_successful_allocs = 0;
    for (i = 0; i < BURST_COUNT; ++i)
    {
      tin = clock_time();
      buf[i] = memb_alloc(&test_mem);
      tout = clock_time();
      if (!buf[i])
      {
        LOG_TIME_CONTIKI("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(clock_time(), "OOM");
        goto cleanup_logic;
      }
      alloc_cnt++;
      LOG_TIME_CONTIKI("burst", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
      current_burst_successful_allocs++;
    }

    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_alloc_%02d", round);
    emit_snapshot_contiki_memb(snap_phase_label);

    for (j = current_burst_successful_allocs - 1; j >= 0; --j)
    {
      tin = clock_time();
      res_free = memb_free(&test_mem, buf[j]);
      tout = clock_time();
      buf[j] = NULL;
      if (res_free == 1)
      {
        free_cnt++;
        LOG_TIME_CONTIKI("burst", "free", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
      }
      else
      {
        LOG_TIME_CONTIKI("burst", "free", BLOCK_SIZE, tin, tout, "ERR_FREE", alloc_cnt, free_cnt);
      }
    }
    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_free_%02d", round);
    emit_snapshot_contiki_memb(snap_phase_label);
  }

cleanup_logic:
  for (i = 0; i < successfully_pinned; ++i)
  {
    if (pinned[i] != NULL)
    {
      t_cleanup_in = clock_time();
      res_free = memb_free(&test_mem, pinned[i]);
      t_cleanup_out = clock_time();
      pinned[i] = NULL;
      if (res_free == 1)
      {
        free_cnt++;
        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
      }
      else
      {
        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "ERR_FREE", alloc_cnt, free_cnt);
      }
    }
  }
  emit_snapshot_contiki_memb("post_cleanup");

  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}