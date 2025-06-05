#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define ALLOCATOR_NAME "newlib-nano"
#define TEST_NAME "LeakExhaust"
#define BLOCK_SIZE 128

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
  void *p;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  while (true)
  {
    uint64_t tin = k_uptime_ticks();
    p = malloc(BLOCK_SIZE);
    uint64_t tout = k_uptime_ticks();
    if (!p)
    {
      P_TIME("leakloop", "malloc", BLOCK_SIZE, tin, tout, "NULL");
      P_FAULT("OOM");
      break;
    }
    alloc_cnt++;
    P_TIME("leakloop", "malloc", BLOCK_SIZE, tin, tout, "OK");
  }

  emit_snapshot("after_leakloop_exhaustion");

  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}