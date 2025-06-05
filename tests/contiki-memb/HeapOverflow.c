#include "contiki.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-memb"
#define TEST_NAME "HeapOverflow"
#define BLOCK_SIZE 128
#define BLOCK_COUNT 3

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

static uint32_t alloc_cnt = 0, free_cnt = 0;
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

PROCESS(heap_overflow_test, "Heap Overflow Test");
AUTOSTART_PROCESSES(&heap_overflow_test);

PROCESS_THREAD(heap_overflow_test, ev, data)
{
  static struct block *A = NULL, *B = NULL, *C = NULL;
  static clock_time_t tin, tout, t_cleanup_in, t_cleanup_out;
  static char *overflow_ptr;
  static int res_free;

  PROCESS_BEGIN();

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_CONTIKI(CLOCK_SECOND);

  memb_init(&test_mem);
  emit_snapshot_contiki_memb("baseline");

  tin = clock_time();
  A = memb_alloc(&test_mem);
  tout = clock_time();

  if (A == NULL)
  {
    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_CONTIKI(clock_time(), "OOM");
    goto done_label;
  }
  alloc_cnt++;
  LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);

  tin = clock_time();
  B = memb_alloc(&test_mem);
  tout = clock_time();

  if (B == NULL)
  {
    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_CONTIKI(clock_time(), "OOM");
    goto cleanup_A_only;
  }
  alloc_cnt++;
  LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
  emit_snapshot_contiki_memb("after_setup");

  overflow_ptr = (char *)A;
  tin = clock_time();
  memset(overflow_ptr, 0xFF, BLOCK_SIZE + 8);
  tout = clock_time();
  LOG_TIME_CONTIKI("hof_write", "memset_overflow", BLOCK_SIZE + 8, tin, tout, "HOF_WRITE_DONE", alloc_cnt, free_cnt);
  emit_snapshot_contiki_memb("post_primitive_trigger");

  tin = clock_time();
  C = memb_alloc(&test_mem);
  tout = clock_time();

  if (C == NULL)
  {
    LOG_TIME_CONTIKI("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_CONTIKI(clock_time(), "OOM");
  }
  else
  {
    alloc_cnt++;
    LOG_TIME_CONTIKI("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    t_cleanup_in = clock_time();
    res_free = memb_free(&test_mem, C);
    t_cleanup_out = clock_time();
    if (res_free == 1)
    {
      free_cnt++;
      LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
    }
    else
    {
      LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "ERR_FREE", alloc_cnt, free_cnt);
    }
    C = NULL;
  }
  emit_snapshot_contiki_memb("after_hof_check_alloc");

  if (B != NULL)
  {
    t_cleanup_in = clock_time();
    res_free = memb_free(&test_mem, B);
    t_cleanup_out = clock_time();
    B = NULL;
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

cleanup_A_only:
  if (A != NULL)
  {
    t_cleanup_in = clock_time();
    res_free = memb_free(&test_mem, A);
    t_cleanup_out = clock_time();
    A = NULL;
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
  emit_snapshot_contiki_memb("post_cleanup");

done_label:
  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  PROCESS_END();
}