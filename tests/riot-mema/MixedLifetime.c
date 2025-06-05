#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALLOCATOR_NAME "riot-mema"
#define TICK_HZ 1000000
#define TEST_NAME "MixedLifetime"
#define NUM_BLOCKS 64
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

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

       void *pinned[PIN_COUNT];
       void *buf[BURST_COUNT];
       uint32_t t1, t2, t3, t4, t5, t6, t7, t8;
       int i, round_idx, j_idx;
       char snap_phase_label[64];
       int successfully_pinned = 0;

       for (i = 0; i < PIN_COUNT; ++i)
              pinned[i] = NULL;
       for (i = 0; i < BURST_COUNT; ++i)
              buf[i] = NULL;

       emit_snapshot_mema("baseline");

       for (i = 0; i < PIN_COUNT; ++i)
       {
              t1 = ztimer_now(ZTIMER_USEC);
              pinned[i] = memarray_alloc(&pool);
              t2 = ztimer_now(ZTIMER_USEC);
              if (!pinned[i])
              {
                     LOG_TIME_RIOT("pin", "malloc", BLOCK_SIZE, t1, t2, "NULL", alloc_cnt, free_cnt);
                     LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
                     goto cleanup_logic;
              }
              alloc_cnt++;
              LOG_TIME_RIOT("pin", "malloc", BLOCK_SIZE, t1, t2, "OK", alloc_cnt, free_cnt);
              successfully_pinned++;
       }
       emit_snapshot_mema("after_pins");

       for (round_idx = 1; round_idx <= BURST_ROUNDS; ++round_idx)
       {
              int current_burst_successful_allocs = 0;
              for (i = 0; i < BURST_COUNT; ++i)
              {
                     t3 = ztimer_now(ZTIMER_USEC);
                     buf[i] = memarray_alloc(&pool);
                     t4 = ztimer_now(ZTIMER_USEC);
                     if (!buf[i])
                     {
                            LOG_TIME_RIOT("burst", "malloc", BLOCK_SIZE, t3, t4, "NULL", alloc_cnt, free_cnt);
                            LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
                            goto cleanup_logic;
                     }
                     alloc_cnt++;
                     LOG_TIME_RIOT("burst", "malloc", BLOCK_SIZE, t3, t4, "OK", alloc_cnt, free_cnt);
                     current_burst_successful_allocs++;
              }

              snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_alloc_%02d", round_idx);
              emit_snapshot_mema(snap_phase_label);

              for (j_idx = current_burst_successful_allocs - 1; j_idx >= 0; --j_idx)
              {
                     t5 = ztimer_now(ZTIMER_USEC);
                     memarray_free(&pool, buf[j_idx]);
                     buf[j_idx] = NULL;
                     t6 = ztimer_now(ZTIMER_USEC);
                     free_cnt++;
                     LOG_TIME_RIOT("burst", "free", BLOCK_SIZE, t5, t6, "OK", alloc_cnt, free_cnt);
              }
              snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_free_%02d", round_idx);
              emit_snapshot_mema(snap_phase_label);
       }

cleanup_logic:
       for (i = 0; i < successfully_pinned; ++i)
       {
              if (pinned[i])
              {
                     t7 = ztimer_now(ZTIMER_USEC);
                     memarray_free(&pool, pinned[i]);
                     pinned[i] = NULL;
                     t8 = ztimer_now(ZTIMER_USEC);
                     free_cnt++;
                     LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, t7, t8, "OK", alloc_cnt, free_cnt);
              }
       }
       emit_snapshot_mema("post_cleanup");

       LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
       return 0;
}