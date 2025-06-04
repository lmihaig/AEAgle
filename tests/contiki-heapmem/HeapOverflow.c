#include "contiki.h"
#include "lib/heapmem.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "HeapOverflow"
#define BLOCK_SIZE 128

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\n", CLOCK_SECOND)
#define P_TIME(ph, op, sz, ti, to, res)                                    \
    printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\n", ph, op, (unsigned)(sz), \
           (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)
#define P_FAULT(res) \
    printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(heap_overflow_test, "Heap Overflow Test");
AUTOSTART_PROCESSES(&heap_overflow_test);

PROCESS_THREAD(heap_overflow_test, ev, data)
{
    static void *A, *B, *C;
    static char *overflow_ptr;
    static clock_time_t tin, tout;

    PROCESS_BEGIN();

    printf("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
    P_META();

    tin = clock_time();
    A = heapmem_alloc(BLOCK_SIZE);
    tout = clock_time();
    if (!A)
    {
        P_TIME("allocA", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto done;
    }
    alloc_cnt++;
    P_TIME("allocA", "malloc", BLOCK_SIZE, tin, tout, "OK");

    tin = clock_time();
    B = heapmem_alloc(BLOCK_SIZE);
    tout = clock_time();
    if (!B)
    {
        P_TIME("allocB", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto freeA;
    }
    alloc_cnt++;
    P_TIME("allocB", "malloc", BLOCK_SIZE, tin, tout, "OK");

    overflow_ptr = (char *)A;
    tin = clock_time();
    memset(overflow_ptr, 0xFF, BLOCK_SIZE + 8); // Overflow
    tout = clock_time();
    P_TIME("overflow", "memset", BLOCK_SIZE + 8, tin, tout, "DONE");

    tin = clock_time();
    C = heapmem_alloc(BLOCK_SIZE);
    tout = clock_time();
    if (!C)
    {
        P_TIME("allocC", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM/CORRUPT");
    }
    else
    {
        alloc_cnt++;
        P_TIME("allocC", "malloc", BLOCK_SIZE, tin, tout, "OK?");
        heapmem_free(C);
        free_cnt++;
    }

    heapmem_free(B);
    free_cnt++;
freeA:
    heapmem_free(A);
    free_cnt++;

done:
    printf("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}
