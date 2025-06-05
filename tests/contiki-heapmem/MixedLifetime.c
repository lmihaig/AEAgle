#include "contiki.h"
#include "lib/heapmem.h"
#include "sys/cc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

#define PRINTF_LOG_CONTIKI(format, ...) printf(format, ##__VA_ARGS__)

#define LOG_TEST_START(alloc_name, test_name_str) \
    PRINTF_LOG_CONTIKI("# %s %s start\r\n", (alloc_name), (test_name_str))

#define LOG_TEST_END(alloc_name, test_name_str) \
    PRINTF_LOG_CONTIKI("# %s %s end\r\n", (alloc_name), (test_name_str))

#define LOG_META_CONTIKI(tick_hz_val) \
    PRINTF_LOG_CONTIKI("META,tick_hz,%u\r\n", (unsigned)(tick_hz_val))

#define LOG_TIME_CONTIKI(phase_str, op_str, size_val, time_in, time_out, result_str, ac, fc) \
    PRINTF_LOG_CONTIKI("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n",                               \
                       (phase_str), (op_str), (unsigned)(size_val),                          \
                       (unsigned long)(time_in), (unsigned long)(time_out), (result_str),    \
                       (unsigned long)(ac), (unsigned long)(fc))

#define LOG_SNAP_CONTIKI(phase_str, free_b_val, allocated_b_val, max_alloc_b_val) \
    PRINTF_LOG_CONTIKI("SNAP,%s,%lu,%lu,%lu\r\n",                                 \
                       (phase_str), (unsigned long)(free_b_val),                  \
                       (unsigned long)(allocated_b_val), (unsigned long)(max_alloc_b_val))

#define LOG_FAULT_CONTIKI(current_ticks, error_str) \
    PRINTF_LOG_CONTIKI("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)(current_ticks), (error_str))

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;
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

PROCESS(mixed_lifetime_test, "Mixed Lifetime Test");
AUTOSTART_PROCESSES(&mixed_lifetime_test);

PROCESS_THREAD(mixed_lifetime_test, ev, data)
{
    static void *pinned[PIN_COUNT];
    static void *buf[BURST_COUNT];
    static clock_time_t tin, tout, t_cleanup_in, t_cleanup_out;
    static int i, round_idx, j_idx;
    static char snap_phase_label[64];
    static int successfully_pinned = 0;
    static int current_burst_successful_allocs = 0;

    PROCESS_BEGIN();

    alloc_cnt = 0;
    free_cnt = 0;
    max_observed_allocated_bytes_heapmem = 0;
    successfully_pinned = 0;

    for (i = 0; i < PIN_COUNT; ++i)
        pinned[i] = NULL;
    for (i = 0; i < BURST_COUNT; ++i)
        buf[i] = NULL;

    LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
    LOG_META_CONTIKI(CLOCK_SECOND);

    emit_snapshot_contiki_heapmem("baseline");

    for (i = 0; i < PIN_COUNT; ++i)
    {
        tin = clock_time();
        pinned[i] = heapmem_alloc(BLOCK_SIZE * 2);
        tout = clock_time();
        if (!pinned[i])
        {
            LOG_TIME_CONTIKI("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "NULL", alloc_cnt, free_cnt);
            LOG_FAULT_CONTIKI(clock_time(), "OOM");
            goto cleanup_logic;
        }
        alloc_cnt++;
        LOG_TIME_CONTIKI("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "OK", alloc_cnt, free_cnt);
        successfully_pinned++;
    }
    emit_snapshot_contiki_heapmem("after_pins");

    for (round_idx = 1; round_idx <= BURST_ROUNDS; ++round_idx)
    {
        current_burst_successful_allocs = 0;
        for (i = 0; i < BURST_COUNT; ++i)
        {
            tin = clock_time();
            buf[i] = heapmem_alloc(BLOCK_SIZE);
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

        snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_alloc_%02d", round_idx);
        emit_snapshot_contiki_heapmem(snap_phase_label);

        for (j_idx = current_burst_successful_allocs - 1; j_idx >= 0; --j_idx)
        {
            tin = clock_time();
            heapmem_free(buf[j_idx]);
            tout = clock_time();
            free_cnt++;

            LOG_TIME_CONTIKI("burst", "free", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
            buf[j_idx] = NULL;
        }
        snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_free_%02d", round_idx);
        emit_snapshot_contiki_heapmem(snap_phase_label);
    }

cleanup_logic:
    for (i = 0; i < successfully_pinned; ++i)
    {
        if (pinned[i] != NULL)
        {
            t_cleanup_in = clock_time();
            heapmem_free(pinned[i]);
            t_cleanup_out = clock_time();
            pinned[i] = NULL;
            free_cnt++;
            LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE * 2, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
        }
    }
    emit_snapshot_contiki_heapmem("post_cleanup");

    LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}