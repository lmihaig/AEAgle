#include <inttypes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>
#include <stdio.h> // Required for snprintf

#define ALLOCATOR_NAME "zephyr"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

K_HEAP_DEFINE(my_heap, 65536);

static uint32_t alloc_cnt, free_cnt;

#define P_META() printk("META,tick_hz,%u\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC)

#define P_TIME(ph, op, sz, ti, to, res)                                  \
  printk("TIME,%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%s,%u,%u\n", ph, op,     \
         (unsigned)(sz), (uint64_t)(ti), (uint64_t)(to), res, alloc_cnt, \
         free_cnt)

#define P_SNAP(ph, st)                                         \
  printk("SNAP,%s,%zu,%zu,%zu\n", ph, (size_t)(st).free_bytes, \
         (size_t)(st).allocated_bytes, (size_t)(st).max_allocated_bytes)

#define P_FAULT(res) \
  printk("FAULT,%" PRIu64 ",0xDEAD,%s\n", (uint64_t)k_uptime_ticks(), res)

static void emit_snapshot(const char *phase)
{
  struct sys_memory_stats st;
  sys_heap_runtime_stats_get(&my_heap.heap, &st);
  P_SNAP(phase, st);
}

int main(void)
{
  void *pinned[PIN_COUNT];
  void *buf[BURST_COUNT];
  char snap_phase_label[64]; // Buffer for dynamic snapshot phase names

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  for (int i = 0; i < PIN_COUNT; ++i)
  {
    uint64_t tin = k_uptime_ticks();
    pinned[i] = k_heap_alloc(&my_heap, BLOCK_SIZE * 2, K_NO_WAIT);
    uint64_t tout = k_uptime_ticks();
    if (!pinned[i])
    {
      P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "NULL");
      P_FAULT("OOM");
      // Attempt to free any already pinned blocks if OOM occurs here
      for (int k = 0; k < i; ++k)
      {
        if (pinned[k])
        {
          // Not logging these frees as it's an exceptional path before full setup
          k_heap_free(&my_heap, pinned[k]);
          // free_cnt++; // decide if this is a "successful free" in this context
        }
      }
      goto done;
    }
    alloc_cnt++;
    P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "OK");
  }
  emit_snapshot("after_pins");

  for (int round = 1; round <= BURST_ROUNDS; ++round)
  {
    int i;
    for (i = 0; i < BURST_COUNT; ++i)
    {
      uint64_t tin = k_uptime_ticks();
      buf[i] = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
      uint64_t tout = k_uptime_ticks();
      if (!buf[i])
      {
        P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        // Note: Potential leak here of buf[0] to buf[i-1] from this round
        goto cleanup;
      }
      alloc_cnt++;
      P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
    }
    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_alloc_%d", round);
    emit_snapshot(snap_phase_label);

    for (int j = i - 1; j >= 0; --j)
    {
      uint64_t tin = k_uptime_ticks();
      k_heap_free(&my_heap, buf[j]);
      uint64_t tout = k_uptime_ticks();
      free_cnt++;
      P_TIME("burst", "free", BLOCK_SIZE, tin, tout, "OK");
    }
    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_free_%d", round);
    emit_snapshot(snap_phase_label);
  }

cleanup:
  for (int i = 0; i < PIN_COUNT; ++i)
  {
    if (pinned[i]) // Ensure pointer is valid before freeing
    {
      uint64_t tin = k_uptime_ticks();
      k_heap_free(&my_heap, pinned[i]);
      uint64_t tout = k_uptime_ticks();
      free_cnt++;
      P_TIME("cleanup", "free", BLOCK_SIZE * 2, tin, tout, "OK");
    }
  }
  emit_snapshot("post_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}