#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define ALLOCATOR_NAME "newlib-nano"
#define TEST_NAME "UseAfterFree"
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

#define P_LEAK(addr) printk("LEAK,%p\n", (void *)addr)
#define P_NOLEAK(addr) printk("NOLEAK,%p\n", (void *)addr)

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
  void *p1 = NULL;
  void *p2 = NULL;
  uint8_t *buf1;
  uint8_t *buf2;
  uint64_t tin, tout;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  tin = k_uptime_ticks();
  p1 = malloc(BLOCK_SIZE);
  tout = k_uptime_ticks();
  if (!p1)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");
  emit_snapshot("after_setup");

  tin = k_uptime_ticks();
  free(p1);
  tout = k_uptime_ticks();
  free_cnt++;
  P_TIME("setup", "free", BLOCK_SIZE, tin, tout, "OK");
  emit_snapshot("after_free1");

  buf1 = (uint8_t *)p1;
  tin = k_uptime_ticks();
  memset(buf1, 0xA5, BLOCK_SIZE);
  tout = k_uptime_ticks();
  P_TIME("uaf_write", "memset_uaf", BLOCK_SIZE, tin, tout, "UAF_WRITE_DONE");

  tin = k_uptime_ticks();
  p2 = malloc(BLOCK_SIZE);
  tout = k_uptime_ticks();
  if (!p2)
  {
    P_TIME("uaf_realloc", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("uaf_realloc", "malloc", BLOCK_SIZE, tin, tout, "OK");
  emit_snapshot("post_primitive_realloc");

  buf2 = (uint8_t *)p2;
  tin = k_uptime_ticks();
  bool leaked = false;
  for (int i = 0; i < BLOCK_SIZE; ++i)
  {
    if (buf2[i] == 0xA5)
    {
      leaked = true;
      break;
    }
  }
  tout = k_uptime_ticks();

  if (leaked)
  {
    P_TIME("uaf_inspect", "inspect_uaf", BLOCK_SIZE, tin, tout, "LEAK_DETECTED");
    P_LEAK(p2);
  }
  else
  {
    P_TIME("uaf_inspect", "inspect_uaf", BLOCK_SIZE, tin, tout, "NO_LEAK_DETECTED");
    P_NOLEAK(p2);
  }

  if (p2)
  {
    uint64_t tin_free_p2 = k_uptime_ticks();
    free(p2);
    uint64_t tout_free_p2 = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin_free_p2, tout_free_p2, "OK");
    p2 = NULL;
  }
  emit_snapshot("post_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}