#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h> // Required for snprintf
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define ALLOCATOR_NAME "newlib-nano"
#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

#define P_META() printk("META,tick_hz,%u\n", CONFIG_SYS_CLOCK_TICKS_PER_SEC)

#define P_TIME(ph, op, sz, tin, tout, res)                                  \
  printk("TIME,%s,%s,%u,%" PRIu64 ",%" PRIu64 ",%s,%u,%u\n", ph, op,        \
         (unsigned)(sz), (uint64_t)(tin), (uint64_t)(tout), res, alloc_cnt, \
         free_cnt)

#define P_SNAP(ph, free_b, alloc_b, max_b)                                 \
  printk("SNAP,%s,%zu,%zu,%zu\n", ph, (size_t)(free_b), (size_t)(alloc_b), \
         (size_t)(max_b))

#define P_FAULT(res) \
  printk("FAULT,%" PRIu64 ",0xDEAD,%s\n", (uint64_t)k_uptime_ticks(), res)

static uint32_t alloc_cnt, free_cnt;
static size_t max_live_bytes;

static void emit_snapshot(const char *phase)
{
  struct mallinfo mi = mallinfo();
  if (mi.uordblks > max_live_bytes)
  {
    max_live_bytes = mi.uordblks;
  }
  P_SNAP(phase, mi.fordblks, mi.uordblks, max_live_bytes);
}

int main(void)
{
  void *pinned[PIN_COUNT];
  void *buf[BURST_COUNT];
  char snap_phase_label[64];
  uint64_t tin, tout;
  int i, round_idx, j_idx; // Declare loop variables outside for goto handling

  for (i = 0; i < PIN_COUNT; ++i)
    pinned[i] = NULL;
  for (i = 0; i < BURST_COUNT; ++i)
    buf[i] = NULL;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  for (i = 0; i < PIN_COUNT; ++i)
  {
    tin = k_uptime_ticks();
    pinned[i] = malloc(BLOCK_SIZE * 2);
    tout = k_uptime_ticks();
    if (!pinned[i])
    {
      P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "NULL");
      P_FAULT("OOM");
      goto cleanup_on_pin_failure;
    }
    alloc_cnt++;
    P_TIME("pin", "malloc", BLOCK_SIZE * 2, tin, tout, "OK");
  }
  emit_snapshot("after_pins");

  for (round_idx = 1; round_idx <= BURST_ROUNDS; ++round_idx)
  {
    int current_burst_successful_allocs = 0;
    for (i = 0; i < BURST_COUNT; ++i)
    {
      tin = k_uptime_ticks();
      buf[i] = malloc(BLOCK_SIZE);
      tout = k_uptime_ticks();
      if (!buf[i])
      {
        P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "NULL");
        P_FAULT("OOM");
        // Note: Potential leak of buf[0] to buf[i-1] from this round
        // A more robust cleanup would free these before jumping.
        goto cleanup_pinned_only;
      }
      alloc_cnt++;
      P_TIME("burst", "malloc", BLOCK_SIZE, tin, tout, "OK");
      current_burst_successful_allocs++;
    }
    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_alloc_%d", round_idx);
    emit_snapshot(snap_phase_label);

    for (j_idx = current_burst_successful_allocs - 1; j_idx >= 0; --j_idx)
    {
      tin = k_uptime_ticks();
      free(buf[j_idx]);
      buf[j_idx] = NULL; // Mark as freed
      tout = k_uptime_ticks();
      free_cnt++;
      P_TIME("burst", "free", BLOCK_SIZE, tin, tout, "OK");
    }
    snprintf(snap_phase_label, sizeof(snap_phase_label), "after_burst_free_%d", round_idx);
    emit_snapshot(snap_phase_label);
  }

cleanup_pinned_only:
cleanup_on_pin_failure: // Label for OOM during pinning or bursting
  for (i = 0; i < PIN_COUNT; ++i)
  {
    if (pinned[i])
    {
      tin = k_uptime_ticks();
      free(pinned[i]);
      pinned[i] = NULL; // Mark as freed
      tout = k_uptime_ticks();
      free_cnt++;
      P_TIME("cleanup", "free", BLOCK_SIZE * 2, tin, tout, "OK");
    }
  }
  emit_snapshot("post_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}