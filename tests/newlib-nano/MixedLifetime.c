#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define P_META() printk("META,tick_hz,%u\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC)

#define P_TIME(ph, op, sz, tin, tout, res)                                     \
  printk("TIME,%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%s,%u,%u\n", ph, op,           \
         (unsigned)(sz), (uint64_t)(tin), (uint64_t)(tout), res, alloc_cnt,    \
         free_cnt)

#define P_SNAP(ph, free_b, alloc_b, max_b)                                     \
  printk("SNAP,%s,%zu,%zu,%zu\n", ph, (size_t)(free_b), (size_t)(alloc_b),     \
         (size_t)(max_b))

#define P_FAULT(res)                                                           \
  printk("FAULT,%" PRIu64 ",0xDEAD,%s\n", (uint64_t)k_uptime_ticks(), res)

static uint32_t alloc_cnt, free_cnt;
static size_t max_live_bytes;

static void emit_snapshot(const char *phase) {
  struct mallinfo mi = mallinfo();
  if (mi.uordblks > max_live_bytes) {
    max_live_bytes = mi.uordblks;
  }
  P_SNAP(phase, mi.fordblks, mi.uordblks, max_live_bytes);
}

#define ALLOCATOR_NAME "newlib-nano"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

int main(void) {
  void *pinned[PIN_COUNT];
  void *buf[BURST_COUNT];

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  for (int i = 0; i < PIN_COUNT; ++i) {
    uint64_t tin = k_uptime_ticks();
    pinned[i] = malloc(BLOCK_SIZE * 2);
    uint64_t tout = k_uptime_ticks();
    if (!pinned[i]) {
      P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "NULL");
      P_FAULT("OOM");
      goto done;
    }
    alloc_cnt++;
    P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "OK");
  }
  emit_snapshot("after_pins");

  for (int round = 1; round <= BURST_ROUNDS; ++round) {
    int i;
    for (i = 0; i < BURST_COUNT; ++i) {
      uint64_t tin = k_uptime_ticks();
      buf[i] = malloc(BLOCK_SIZE);
      uint64_t tout = k_uptime_ticks();
      if (!buf[i]) {
        P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        goto cleanup;
      }
      alloc_cnt++;
      P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
    }
    emit_snapshot("after_burst_alloc");

    for (int j = i - 1; j >= 0; --j) {
      uint64_t tin = k_uptime_ticks();
      free(buf[j]);
      uint64_t tout = k_uptime_ticks();
      free_cnt++;
      P_TIME("burst", "free", BLOCK_SIZE, tin, tout, "OK");
    }
    emit_snapshot("after_burst_free");
  }

cleanup:
  for (int i = 0; i < PIN_COUNT; ++i) {
    uint64_t tin = k_uptime_ticks();
    free(pinned[i]);
    uint64_t tout = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE * 2, tin, tout, "OK");
  }
  emit_snapshot("after_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
