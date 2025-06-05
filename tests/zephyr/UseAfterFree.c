#include <inttypes.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>
#include <stdio.h> // For printk with %p if not implicitly handled by Zephyr's printk

#define ALLOCATOR_NAME "zephyr"
#define TEST_NAME "UseAfterFree"
#define BLOCK_SIZE 128

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

#define P_LEAK(addr) printk("LEAK,%p\n", (void *)addr)
#define P_NOLEAK(addr) printk("NOLEAK,%p\n", (void *)addr)

static void emit_snapshot(const char *phase)
{
  struct sys_memory_stats st;
  sys_heap_runtime_stats_get(&my_heap.heap, &st);
  P_SNAP(phase, st);
}

int main(void)
{
  void *p1 = NULL;
  void *p2 = NULL;
  uint8_t *buf1;
  uint8_t *buf2;
  // const uint8_t PATTERN = 0x5A; // Original pattern not used in UAF check logic with 0xA5
  uint64_t tin, tout;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  tin = k_uptime_ticks();
  p1 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
  tout = k_uptime_ticks();
  if (!p1)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");
  // memset(p1, PATTERN, BLOCK_SIZE); // Initial pattern setting, not directly part of logged UAF steps
  emit_snapshot("after_setup");

  tin = k_uptime_ticks();
  k_heap_free(&my_heap, p1);
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
  p2 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
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
  tin = k_uptime_ticks(); // For inspect timing
  bool leaked = false;
  for (int i = 0; i < BLOCK_SIZE; ++i)
  {
    if (buf2[i] == 0xA5)
    {
      leaked = true;
      break;
    }
  }
  tout = k_uptime_ticks(); // For inspect timing

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
  { // Check if p2 was allocated before attempting to free
    tin = k_uptime_ticks();
    k_heap_free(&my_heap, p2);
    tout = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin, tout, "OK");
  }
  emit_snapshot("post_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}