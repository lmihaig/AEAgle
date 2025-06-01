#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define ALLOCATOR_NAME "newlib"
#define TEST_NAME "BurstMalloc"
#define BLOCK_SIZE 128
#define BURST_ROUNDS 10
#define BURST_COUNT 10

static uint32_t alloc_cnt, free_cnt;
static size_t max_live_bytes;

#define P_META() printk("META,tick_hz,%u\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC)

#define P_TIME(ph, op, sz, ti, to, res)                                        \
  printk("TIME,%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%s,%u,%u\n", ph, op,           \
         (unsigned)(sz), (uint64_t)(ti), (uint64_t)(to), res, alloc_cnt,       \
         free_cnt)

#define P_SNAP(ph, free, live, max)                                            \
  printk("SNAP,%s,%zu,%zu,%zu\n", ph, (size_t)(free), (size_t)(live),          \
         (size_t)(max))

#define P_FAULT(res)                                                           \
  printk("FAULT,%" PRIu64 ",0xDEAD,%s\n", (uint64_t)k_uptime_ticks(), res)

static void emit_snapshot(const char *phase) {
  struct mallinfo mi = mallinfo();

  if (mi.uordblks > max_live_bytes) {
    max_live_bytes = mi.uordblks;
  }
  P_SNAP(phase, mi.fordblks, mi.uordblks, max_live_bytes);
}

int main(void) {
  void *buf[BURST_COUNT];

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  for (int round = 1; round <= BURST_ROUNDS; ++round) {

    int i;
    for (i = 0; i < BURST_COUNT; ++i) {
      uint64_t tin = k_uptime_ticks();
      buf[i] = malloc(BLOCK_SIZE);
      uint64_t tout = k_uptime_ticks();

      if (!buf[i]) {
        P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto done;
      }

      ++alloc_cnt;
      P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
    }

    emit_snapshot("after_alloc");

    for (int j = i - 1; j >= 0; --j) {
      uint64_t tin = k_uptime_ticks();
      free(buf[j]);
      uint64_t tout = k_uptime_ticks();

      ++free_cnt;
      P_TIME("free", "free", BLOCK_SIZE, tin, tout, "OK");
    }

    emit_snapshot("after_free");
  }

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
}
