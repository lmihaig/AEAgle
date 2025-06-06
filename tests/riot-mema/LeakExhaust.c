#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "riot-mema"
#define TICK_HZ 1000000
#define TEST_NAME "LeakExhaust"
#define NUM_BLOCKS 256
#define BLOCK_SIZE 128

#define PRINTF_LOG_RIOT(format, ...) \
  do                                 \
  {                                  \
    printf(format, ##__VA_ARGS__);   \
    fflush(stdout);                  \
  } while (0)

#define LOG_TEST_START(alloc_name, test_name_str) \
  PRINTF_LOG_RIOT("\r\n# %s %s start\r\n", (alloc_name), (test_name_str))

#define LOG_TEST_END(alloc_name, test_name_str) \
  PRINTF_LOG_RIOT("# %s %s end\r\n", (alloc_name), (test_name_str))

#define LOG_META_RIOT(tick_hz_val) \
  PRINTF_LOG_RIOT("META,tick_hz,%u\r\n", (unsigned)(tick_hz_val))

#define LOG_TIME_RIOT(phase_str, op_str, size_val, time_in, time_out, result_str, ac, fc) \
  PRINTF_LOG_RIOT("TIME,%s,%s,%u,%u,%u,%s,%lu,%lu\r\n",                                   \
                  (phase_str), (op_str), (unsigned)(size_val),                            \
                  (unsigned)(time_in), (unsigned)(time_out), (result_str),                \
                  (unsigned long)(ac), (unsigned long)(fc))

#define LOG_SNAP_RIOT(phase_str, free_b_val, allocated_b_val, max_alloc_b_val) \
  PRINTF_LOG_RIOT("SNAP,%s,%u,%u,%u\r\n",                                      \
                  (phase_str), (unsigned)(free_b_val),                         \
                  (unsigned)(allocated_b_val), (unsigned)(max_alloc_b_val))

#define LOG_FAULT_RIOT(current_ticks, error_str) \
  PRINTF_LOG_RIOT("FAULT,%u,0xDEAD,%s\r\n", (unsigned)(current_ticks), (error_str))

static uint8_t pool_data[NUM_BLOCKS * BLOCK_SIZE];
static memarray_t pool;

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;
static size_t max_allocated_bytes_mema = 0;

static void emit_snapshot_mema(const char *phase)
{
  size_t free_blocks = memarray_available(&pool);
  size_t used_blocks = NUM_BLOCKS - free_blocks;
  size_t current_allocated_bytes = used_blocks * BLOCK_SIZE;
  size_t current_free_bytes = free_blocks * BLOCK_SIZE;

  if (current_allocated_bytes > max_allocated_bytes_mema)
  {
    max_allocated_bytes_mema = current_allocated_bytes;
  }
  LOG_SNAP_RIOT(phase, current_free_bytes, current_allocated_bytes, max_allocated_bytes_mema);
}

int main(void)
{
  memarray_init(&pool, pool_data, BLOCK_SIZE, NUM_BLOCKS);

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_RIOT(TICK_HZ);

  void *arr[NUM_BLOCKS + 1];
  uint32_t t1, t2, t_free_in, t_free_out;

  emit_snapshot_mema("baseline");

  while (1)
  {
    t1 = ztimer_now(ZTIMER_USEC);
    void *p = memarray_alloc(&pool);
    t2 = ztimer_now(ZTIMER_USEC);

    if (!p)
    {
      LOG_TIME_RIOT("leakloop", "malloc", BLOCK_SIZE, t1, t2, "NULL", alloc_cnt, free_cnt);
      LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
      break;
    }
    arr[alloc_cnt] = p;
    alloc_cnt++;
    LOG_TIME_RIOT("leakloop", "malloc", BLOCK_SIZE, t1, t2, "OK", alloc_cnt, free_cnt);
  }

  emit_snapshot_mema("after_leakloop_exhaustion");

  for (uint32_t i = 0; i < alloc_cnt; i++)
  {
    if (arr[i])
    {
      t_free_in = ztimer_now(ZTIMER_USEC);
      memarray_free(&pool, arr[i]);
      t_free_out = ztimer_now(ZTIMER_USEC);
      free_cnt++;
      LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, t_free_in, t_free_out, "OK", alloc_cnt, free_cnt);
    }
  }

  if (alloc_cnt > 0)
  {
    emit_snapshot_mema("post_cleanup");
  }

  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  return 0;
}