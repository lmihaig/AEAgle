#include "contiki.h"
#include "sys/rtimer.h"
#include "lib/heapmem.h"
#include "sys/cc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "HeapOverflow"
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

PROCESS(heap_overflow_test, "Heap Overflow Test");
AUTOSTART_PROCESSES(&heap_overflow_test);

PROCESS_THREAD(heap_overflow_test, ev, data)
{
    static void *A = NULL, *B = NULL, *C = NULL;
    static char *overflow_ptr;
    static rtimer_clock_t tin, tout, t_cleanup_in, t_cleanup_out;

    PROCESS_BEGIN();

    alloc_cnt = 0;
    free_cnt = 0;

    max_observed_allocated_bytes_heapmem = 0;

    LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);

        LOG_META_CONTIKI(RTIMER_SECOND);

    emit_snapshot_contiki_heapmem("baseline");

    tin = RTIMER_NOW();
    A = heapmem_alloc(BLOCK_SIZE);
    tout = RTIMER_NOW();
    if (!A)
    {
        LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
        goto done_label;
    }
    alloc_cnt++;

    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);

    tin = RTIMER_NOW();
    B = heapmem_alloc(BLOCK_SIZE);
    tout = RTIMER_NOW();
    if (!B)
    {
        LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
        goto cleanup_A_only;
    }
    alloc_cnt++;

    LOG_TIME_CONTIKI("setup", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);
    emit_snapshot_contiki_heapmem("after_setup");

    overflow_ptr = (char *)A;
    tin = RTIMER_NOW();
    memset(overflow_ptr, 0xFF, BLOCK_SIZE + 8);
    tout = RTIMER_NOW();
    LOG_TIME_CONTIKI("hof_write", "memset_overflow", BLOCK_SIZE + 8, tin, tout, "HOF_WRITE_DONE", alloc_cnt, free_cnt);
    emit_snapshot_contiki_heapmem("post_primitive_trigger");

    tin = RTIMER_NOW();
    C = heapmem_alloc(BLOCK_SIZE);
    tout = RTIMER_NOW();
    if (!C)
    {
        LOG_TIME_CONTIKI("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "NULL", alloc_cnt, free_cnt);
        LOG_FAULT_CONTIKI(RTIMER_NOW(), "OOM");
    }
    else
    {
        alloc_cnt++;

        LOG_TIME_CONTIKI("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "OK", alloc_cnt, free_cnt);

        t_cleanup_in = RTIMER_NOW();
        heapmem_free(C);
        t_cleanup_out = RTIMER_NOW();
        free_cnt++;

        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
        C = NULL;
    }
    emit_snapshot_contiki_heapmem("after_hof_check_alloc");

    if (B != NULL)
    {
        t_cleanup_in = RTIMER_NOW();
        heapmem_free(B);
        t_cleanup_out = RTIMER_NOW();
        B = NULL;
        free_cnt++;

        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
    }

cleanup_A_only:
    if (A != NULL)
    {
        t_cleanup_in = RTIMER_NOW();
        heapmem_free(A);
        t_cleanup_out = RTIMER_NOW();
        A = NULL;
        free_cnt++;
        LOG_TIME_CONTIKI("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
    }
    emit_snapshot_contiki_heapmem("post_cleanup");

done_label:
    LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}