#include <inttypes.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/mem_stats.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

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

static void emit_snapshot(const char *phase)
{
  struct sys_memory_stats st;
  sys_heap_runtime_stats_get(&my_heap.heap, &st);
  P_SNAP(phase, st);
}

int main(void)
{
  void *p1, *p2;
  uint8_t *buf1, *buf2;
  const uint8_t PATTERN = 0x5A;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  {
    uint64_t tin = k_uptime_ticks();
    p1 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
    uint64_t tout = k_uptime_ticks();
    if (!p1)
    {
      P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "NULL");
      P_FAULT("OOM");
      goto done;
    }
    alloc_cnt++;
    P_TIME("alloc1", "malloc", BLOCK_SIZE, tin, tout, "OK");

    memset(p1, PATTERN, BLOCK_SIZE);
  }
  emit_snapshot("after_alloc1");

  {
    uint64_t tin = k_uptime_ticks();
    k_heap_free(&my_heap, p1);
    uint64_t tout = k_uptime_ticks();
    free_cnt++;
    P_TIME("free1", "free", BLOCK_SIZE, tin, tout, "OK");
  }
  emit_snapshot("after_free1");

  buf1 = (uint8_t *)p1;
  {
    uint64_t tin = k_uptime_ticks();
    memset(buf1, 0xA5, BLOCK_SIZE); // overwrite
    uint64_t tout = k_uptime_ticks();
    P_TIME("uaf", "memset", BLOCK_SIZE, tin, tout, "DONE");
  }

  {
    uint64_t tin = k_uptime_ticks();
    p2 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
    uint64_t tout = k_uptime_ticks();
    if (!p2)
    {
      P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "NULL");
      P_FAULT("OOM");
      goto done;
    }
    alloc_cnt++;
    P_TIME("alloc2", "malloc", BLOCK_SIZE, tin, tout, "OK");
  }
  emit_snapshot("after_alloc2");

  buf2 = (uint8_t *)p2;
  {
    bool leaked = false;
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
      if (buf2[i] == 0xA5)
      {
        leaked = true;
        break;
      }
    }
    if (leaked)
    {
      P_TIME("inspect", "check", BLOCK_SIZE, k_uptime_ticks(), k_uptime_ticks(),
             "LEAKED");
    }
    else
    {
      P_TIME("inspect", "check", BLOCK_SIZE, k_uptime_ticks(), k_uptime_ticks(),
             "CLEAN");
    }
  }

  k_heap_free(&my_heap, p2);
  free_cnt++;
  emit_snapshot("after_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
