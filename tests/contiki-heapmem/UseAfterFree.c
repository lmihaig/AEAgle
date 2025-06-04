#include "contiki.h"
#include "lib/heapmem.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "UseAfterFree"
#define BLOCK_SIZE 128

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\r\n", CLOCK_SECOND)

#define P_TIME(ph, op, sz, ti, to, res)                                      \
    printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\r\n", ph, op, (unsigned)(sz), \
           (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)

#define P_FAULT(res) \
    printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(use_after_free_test, "Use After Free Test");
AUTOSTART_PROCESSES(&use_after_free_test);

PROCESS_THREAD(use_after_free_test, ev, data)
{
    static void *p1, *p2;
    static uint8_t *buf1, *buf2;
    static const uint8_t PATTERN = 0x5A;
    static clock_time_t tin, tout;

    PROCESS_BEGIN();

    printf("# %s %s start\r\r\n", ALLOCATOR_NAME, TEST_NAME);
    P_META();

    // Allocate and initialize p1
    tin = clock_time();
    p1 = heapmem_alloc(BLOCK_SIZE);
    tout = clock_time();
    if (!p1)
    {
        P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto done;
    }
    alloc_cnt++;
    P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "OK");
    memset(p1, PATTERN, BLOCK_SIZE);

    // Free p1
    tin = clock_time();
    heapmem_free(p1);
    tout = clock_time();
    free_cnt++;
    P_TIME("free1", "free", BLOCK_SIZE, tin, tout, "OK");

    // Use-after-free: write to p1
    buf1 = (uint8_t *)p1;
    tin = clock_time();
    memset(buf1, 0xA5, BLOCK_SIZE); // overwrite
    tout = clock_time();
    P_TIME("uaf", "memset", BLOCK_SIZE, tin, tout, "DONE");

    // Allocate p2
    tin = clock_time();
    p2 = heapmem_alloc(BLOCK_SIZE);
    tout = clock_time();
    if (!p2)
    {
        P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto done;
    }
    alloc_cnt++;
    P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "OK");

    // Check for leaked data
    buf2 = (uint8_t *)p2;
    bool leaked = false;
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
        if (buf2[i] == 0xA5)
        {
            leaked = true;
            break;
        }
    }
    P_TIME("inspect", "check", BLOCK_SIZE, clock_time(), clock_time(),
           leaked ? "LEAKED" : "CLEAN");

    // Free p2
    tin = clock_time();
    heapmem_free(p2);
    tout = clock_time();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, "OK");

done:
    printf("# %s %s end\r\r\n", ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}
