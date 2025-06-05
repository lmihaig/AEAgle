#include <inttypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h> // Required for memset
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define ALLOCATOR_NAME "newlib-nano"
#define TEST_NAME "HeapOverflow"
#define BLOCK_SIZE 128
#define OVERSHOOT 16

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
  void *A = NULL;
  void *B = NULL;
  void *C = NULL;
  uint64_t tin, tout;

  printk("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  tin = k_uptime_ticks();
  A = malloc(BLOCK_SIZE);
  tout = k_uptime_ticks();
  if (!A)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto done;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");

  tin = k_uptime_ticks();
  B = malloc(BLOCK_SIZE);
  tout = k_uptime_ticks();
  if (!B)
  {
    P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
    goto cleanup_A;
  }
  alloc_cnt++;
  P_TIME("setup", "malloc", BLOCK_SIZE, tin, tout, "OK");

  emit_snapshot("after_setup");

  tin = k_uptime_ticks();
  memset(A, 0xFF, BLOCK_SIZE + OVERSHOOT);
  tout = k_uptime_ticks();
  P_TIME("hof_write", "memset_overflow", BLOCK_SIZE + OVERSHOOT, tin, tout, "HOF_WRITE_DONE");

  emit_snapshot("post_primitive_trigger");

  tin = k_uptime_ticks();
  C = malloc(BLOCK_SIZE);
  tout = k_uptime_ticks();
  if (!C)
  {
    P_TIME("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "NULL");
    P_FAULT("OOM");
  }
  else
  {
    alloc_cnt++;
    P_TIME("hof_check_alloc", "malloc", BLOCK_SIZE, tin, tout, "OK");

    uint64_t tin_free_c = k_uptime_ticks();
    free(C);
    uint64_t tout_free_c = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin_free_c, tout_free_c, "OK");
    C = NULL;
  }
  emit_snapshot("after_hof_check_alloc");

  if (B)
  {
    uint64_t tin_free_b = k_uptime_ticks();
    free(B);
    uint64_t tout_free_b = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin_free_b, tout_free_b, "OK");
    B = NULL;
  }

cleanup_A:
  if (A)
  {
    uint64_t tin_free_a = k_uptime_ticks();
    free(A);
    uint64_t tout_free_a = k_uptime_ticks();
    free_cnt++;
    P_TIME("cleanup", "free", BLOCK_SIZE, tin_free_a, tout_free_a, "OK");
    A = NULL;
  }
  emit_snapshot("post_cleanup");

done:
  printk("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}