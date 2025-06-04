#include "contiki.h"
#include "lib/heapmem.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define ALLOCATOR_NAME "contiki-heapmem"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

static uint32_t alloc_cnt = 0, free_cnt = 0;

#define P_META() printf("META,tick_hz,%u\r\r\n", CLOCK_SECOND)

#define P_TIME(ph, op, sz, ti, to, res)                                      \
    printf("TIME,%s,%s,%u,%lu,%lu,%s,%lu,%lu\r\r\n", ph, op, (unsigned)(sz), \
           (unsigned long)(ti), (unsigned long)(to), res, alloc_cnt, free_cnt)

#define P_FAULT(res) \
    printf("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)clock_time(), res)

PROCESS(mixed_lifetime_test, "Mixed Lifetime Test");
AUTOSTART_PROCESSES(&mixed_lifetime_test);

PROCESS_THREAD(mixed_lifetime_test, ev, data)
{
    static void *pinned[PIN_COUNT];
    static void *buf[BURST_COUNT];
    static clock_time_t tin, tout;

    PROCESS_BEGIN();

    printf("# %s %s start\r\r\n", ALLOCATOR_NAME, TEST_NAME);
    P_META();

    // Pin allocations
    for (int i = 0; i < PIN_COUNT; ++i)
    {
        tin = clock_time();
        pinned[i] = heapmem_alloc(BLOCK_SIZE * 2);
        tout = clock_time();
        if (!pinned[i])
        {
            P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "NULL");
            P_FAULT("OOM");
            goto done;
        }
        alloc_cnt++;
        P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "OK");
    }

    // Burst allocation/free rounds
    for (int round = 1; round <= BURST_ROUNDS; ++round)
    {
        int i;
        for (i = 0; i < BURST_COUNT; ++i)
        {
            tin = clock_time();
            buf[i] = heapmem_alloc(BLOCK_SIZE);
            tout = clock_time();
            if (!buf[i])
            {
                P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
                P_FAULT("OOM");
                goto cleanup;
            }
            alloc_cnt++;
            P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
        }

        for (int j = i - 1; j >= 0; --j)
        {
            tin = clock_time();
            heapmem_free(buf[j]);
            tout = clock_time();
            free_cnt++;
            P_TIME("burst", "free", BLOCK_SIZE, tin, tout, "OK");
        }
    }

cleanup:
    for (int i = 0; i < PIN_COUNT; ++i)
    {
        tin = clock_time();
        heapmem_free(pinned[i]);
        tout = clock_time();
        free_cnt++;
        P_TIME("cleanup", "free", BLOCK_SIZE * 2, tin, tout, "OK");
    }

done:
    printf("# %s %s end\r\r\n", ALLOCATOR_NAME, TEST_NAME);
    PROCESS_END();
}
