#include "malloc_monitor.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "riot-tlsf"
#define TICK_HZ 1000000
#define TEST_NAME "HeapOverflow"
#define HEAP_SIZE 65536
#define BLOCK_SIZE 64
#define OVERSHOOT 16

#define PRINTF_LOG_RIOT(format, ...)         \
       do                                    \
       {                                     \
              printf(format, ##__VA_ARGS__); \
              fflush(stdout);                \
       } while (0)

#define LOG_TEST_START(alloc_name, test_name_str) \
       PRINTF_LOG_RIOT("\r\n# %s %s start\r\n", (alloc_name), (test_name_str))

#define LOG_TEST_END(alloc_name, test_name_str) \
       PRINTF_LOG_RIOT("# %s %s end\r\n", (alloc_name), (test_name_str))

#define LOG_META_RIOT(tick_hz_val) \
       PRINTF_LOG_RIOT("META,tick_hz,%u\r\n", (unsigned)(tick_hz_val))

#define LOG_TIME_RIOT(phase_str, op_str, size_val, time_in, time_out, result_str, ac, fc) \
       PRINTF_LOG_RIOT("TIME,%s,%s,%u,%u,%u,%s,%lu,%lu\r\n",                              \
                       (phase_str), (op_str), (unsigned)(size_val),                       \
                       (unsigned)(time_in), (unsigned)(time_out), (result_str),           \
                       (unsigned long)(ac), (unsigned long)(fc))

#define LOG_SNAP_RIOT(phase_str, free_b_val, allocated_b_val, max_alloc_b_val) \
       PRINTF_LOG_RIOT("SNAP,%s,%u,%u,%u\r\n",                                 \
                       (phase_str), (unsigned)(free_b_val),                    \
                       (unsigned)(allocated_b_val), (unsigned)(max_alloc_b_val))

#define LOG_FAULT_RIOT(current_ticks, error_str) \
       PRINTF_LOG_RIOT("FAULT,%u,0xDEAD,%s\r\n", (unsigned)(current_ticks), (error_str))

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;

static void emit_snapshot_riot(const char *phase)
{
       size_t current_usage = malloc_monitor_get_usage_current();
       size_t high_watermark = malloc_monitor_get_usage_high_watermark();
       LOG_SNAP_RIOT(phase, 0, current_usage, high_watermark);
}

int main(void)
{
       malloc_monitor_reset_high_watermark();

       LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
       LOG_META_RIOT(TICK_HZ);

       uint32_t t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
       void *A = NULL;
       void *B = NULL;
       size_t corrupt_size = BLOCK_SIZE + OVERSHOOT;

       emit_snapshot_riot("baseline");

       t1 = ztimer_now(ZTIMER_USEC);
       A = malloc(BLOCK_SIZE);
       t2 = ztimer_now(ZTIMER_USEC);
       if (A)
       {
              alloc_cnt++;
              LOG_TIME_RIOT("setup", "malloc", BLOCK_SIZE, t1, t2, "OK", alloc_cnt, free_cnt);
       }
       else
       {
              LOG_TIME_RIOT("setup", "malloc", BLOCK_SIZE, t1, t2, "NULL", alloc_cnt, free_cnt);
              LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
              LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }

       t3 = ztimer_now(ZTIMER_USEC);
       B = malloc(BLOCK_SIZE);
       t4 = ztimer_now(ZTIMER_USEC);
       if (B)
       {
              alloc_cnt++;
              LOG_TIME_RIOT("setup", "malloc", BLOCK_SIZE, t3, t4, "OK", alloc_cnt, free_cnt);
       }
       else
       {
              LOG_TIME_RIOT("setup", "malloc", BLOCK_SIZE, t3, t4, "NULL", alloc_cnt, free_cnt);
              LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
              if (A)
              {
                     uint32_t tc_a_tin = ztimer_now(ZTIMER_USEC);
                     free(A);
                     uint32_t tc_a_tout = ztimer_now(ZTIMER_USEC);
                     free_cnt++;
                     LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, tc_a_tin, tc_a_tout, "OK", alloc_cnt, free_cnt);
                     A = NULL;
              }
              emit_snapshot_riot("post_cleanup");
              LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
              return 0;
       }
       emit_snapshot_riot("after_setup");

       t5 = ztimer_now(ZTIMER_USEC);
       memset(A, 0xFF, corrupt_size);
       t6 = ztimer_now(ZTIMER_USEC);
       LOG_TIME_RIOT("hof_write", "memset_overflow", corrupt_size, t5, t6, "HOF_WRITE_DONE", alloc_cnt, free_cnt);

       emit_snapshot_riot("post_primitive_trigger");

       t7 = ztimer_now(ZTIMER_USEC);
       free(B);
       B = NULL;
       t8 = ztimer_now(ZTIMER_USEC);
       LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, t7, t8, "OK", alloc_cnt, free_cnt);

       emit_snapshot_riot("after_free_B");

       t9 = ztimer_now(ZTIMER_USEC);
       free(A);
       A = NULL;
       t10 = ztimer_now(ZTIMER_USEC);
       free_cnt++;
       LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, t9, t10, "OK", alloc_cnt, free_cnt);

       emit_snapshot_riot("post_cleanup");

       LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
       return 0;
}