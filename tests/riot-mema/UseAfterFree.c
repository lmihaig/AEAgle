#include "memarray.h"
#include "ztimer.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ALLOCATOR_NAME "riot-mema"
#define TICK_HZ 1000000
#define TEST_NAME "UseAfterFree"
#define NUM_BLOCKS 32
#define BLOCK_SIZE 64

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

#define LOG_LEAK_RIOT(addr_val) \
       PRINTF_LOG_RIOT("LEAK,%p\r\n", (void *)(addr_val))

#define LOG_NOLEAK_RIOT(addr_val) \
       PRINTF_LOG_RIOT("NOLEAK,%p\r\n", (void *)(addr_val))

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

       uint32_t t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t_inspect_in, t_inspect_out;
       void *ptr = NULL;
       void *newptr = NULL;
       uint8_t *buf_check;

       emit_snapshot_mema("baseline");

       t1 = ztimer_now(ZTIMER_USEC);
       ptr = memarray_alloc(&pool);
       t2 = ztimer_now(ZTIMER_USEC);
       if (ptr)
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
       emit_snapshot_mema("after_setup");

       t3 = ztimer_now(ZTIMER_USEC);
       memarray_free(&pool, ptr);
       t4 = ztimer_now(ZTIMER_USEC);
       free_cnt++;
       LOG_TIME_RIOT("setup", "free", BLOCK_SIZE, t3, t4, "OK", alloc_cnt, free_cnt);
       emit_snapshot_mema("after_free1");

       t5 = ztimer_now(ZTIMER_USEC);
       memset(ptr, 0xAA, BLOCK_SIZE);
       t6 = ztimer_now(ZTIMER_USEC);
       LOG_TIME_RIOT("uaf_write", "memset_uaf", BLOCK_SIZE, t5, t6, "UAF_WRITE_DONE", alloc_cnt, free_cnt);
       emit_snapshot_mema("after_uaf_write");

       t7 = ztimer_now(ZTIMER_USEC);
       newptr = memarray_alloc(&pool);
       t8 = ztimer_now(ZTIMER_USEC);
       if (newptr)
       {
              alloc_cnt++;
              LOG_TIME_RIOT("uaf_realloc", "malloc", BLOCK_SIZE, t7, t8, "OK", alloc_cnt, free_cnt);
       }
       else
       {
              LOG_TIME_RIOT("uaf_realloc", "malloc", BLOCK_SIZE, t7, t8, "NULL", alloc_cnt, free_cnt);
              LOG_FAULT_RIOT(ztimer_now(ZTIMER_USEC), "OOM");
              goto cleanup_newptr;
       }

       t_inspect_in = ztimer_now(ZTIMER_USEC);
       bool leaked = false;
       buf_check = (uint8_t *)newptr;
       for (int k = 0; k < BLOCK_SIZE; ++k)
       {
              if (buf_check[k] == 0xAA)
              {
                     leaked = true;
                     break;
              }
       }
       t_inspect_out = ztimer_now(ZTIMER_USEC);

       if (leaked)
       {
              LOG_TIME_RIOT("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "LEAK_DETECTED", alloc_cnt, free_cnt);
              LOG_LEAK_RIOT(newptr);
       }
       else
       {
              LOG_TIME_RIOT("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "NO_LEAK_DETECTED", alloc_cnt, free_cnt);
              LOG_NOLEAK_RIOT(newptr);
       }
       emit_snapshot_mema("post_primitive_realloc");

cleanup_newptr:
       if (newptr)
       {
              t9 = ztimer_now(ZTIMER_USEC);
              memarray_free(&pool, newptr);
              newptr = NULL;
              t10 = ztimer_now(ZTIMER_USEC);
              free_cnt++;
              LOG_TIME_RIOT("cleanup", "free", BLOCK_SIZE, t9, t10, "OK", alloc_cnt, free_cnt);
       }
       emit_snapshot_mema("post_cleanup");

       LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
       return 0;
}