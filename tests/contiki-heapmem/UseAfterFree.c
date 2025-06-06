#include "contiki.h"
#include "sys/rtimer.h"
#include "lib/heapmem.h"
#include "sys/cc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "UseAfterFree"
#define BLOCK_SIZE 128

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

#define LOG_LEAK_CONTIKI(addr_val) \
    PRINTF_LOG_CONTIKI("LEAK,%p\r\n", (void *)(addr_val))

#define LOG_NOLEAK_CONTIKI(addr_val) \
    PRINTF_LOG_CONTIKI("NOLEAK,%p\r\n", (void *)(addr_val))

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

PROCESS(use_after_free_test, "Use After Free Test");
AUTOSTART_PROCESSES(&use_after_free_test);

PROCESS_THREAD(use_after_free_test, ev, data)
{
    static void *p1 = NULL, *p2 = NULL;
    static uint8_t *buf1;
    static const uint8_t PATTERN = 0x5A;
    static rtimer_clock_t tin, tout, t_inspect_in, t_inspect_out, t_cleanup_in, t_cleanup_out;
    static int i;
    static bool leaked;

    PROCESS_BEGIN();

    alloc_cnt = 0;
    free_cnt = 0;
    max_observed_allocated_bytes_heapmem = 0;

    LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);

        LOG_META_CONTIKI(RTIMER_SECOND);

    emit_snapshot_contiki_heapmem("baseline");

    tin = RTIMER_NOW();
    p1 = heapmem_alloc(BLOCK_SIZE);
    tout = RTIMER_NOW();
    if (!p1)
    {
        LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
        goto done_label;
    }
    alloc_cnt++;

    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    memset(p1, PATTERN, BLOCK_SIZE);
    emit_snapshot_contiki_heapmem("after_setup");

    tin = RTIMER_NOW();
    heapmem_free(p1);
    tout = RTIMER_NOW();
    free_cnt++;

    LOG_TIME_CONTIKI("setup", "free", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    emit_snapshot_contiki_heapmem("after_free1");

    buf1 = (uint8_t *)p1;
    tin = RTIMER_NOW();
    memset(buf1, 0xA5, BLOCK_SIZE);
    tout = RTIMER_NOW();
    LOG_TIME_CONTIKI("uaf_write", "memset_uaf", BLOCK_SIZE, tin, tout, "UAF_WRITE_DONE", alloc_cnt, free_cnt);
    emit_snapshot_contiki_heapmem("after_uaf_write");

    tin = RTIMER_NOW();
    p2 = heapmem_alloc(BLOCK_SIZE);
    tout = RTIMER_NOW();
    if (!p2)
    {
        LOG_TIME_CONTIKI("uaf_realloc", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
        goto done_label;
    }
    alloc_cnt++;

    LOG_TIME_CONTIKI("uaf_realloc", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);

    t_inspect_in = RTIMER_NOW();
    leaked = false;
    for (i = 0; i < BLOCK_SIZE; ++i)
    {
        if (((uint8_t *)p2)[i] == 0xA5)
        {
            leaked = true;
            break;
        }
    }
    t_inspect_out = RTIMER_NOW();

    if (leaked)
    {
        LOG_TIME_CONTIKI("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "LEAK_DETECTED", alloc_cnt, free_cnt);
        LOG_LEAK_CONTIKI(p2);
    }
    else
    {
        LOG_TIME_CONTIKI("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "NO_LEAK_DETECTED", alloc_cnt, free_cnt);
        LOG_NOLEAK_CONTIKI(p2);
    }
    emit_snapshot_contiki_heapmem("post_primitive_realloc");

    if (p2 != NULL)
    {
        t_cleanup_in = RTIMER_NOW();
        heapmem_free(p2);
        t_cleanup_out = RTIMER_NOW();
        p2 = NULL;
        free_cnt++;

        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
    }
    emit_snapshot_contiki_heapmem("post_cleanup");

done_label:
    LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}