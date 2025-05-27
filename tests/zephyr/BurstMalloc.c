#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#define OS_NAME "Zephyr OS"
#define TEST_NAME "BurstMallocFree"
#define BLOCK_SIZE 128

#define BURST_ROUNDS 10
#define BURST_COUNT 10

K_HEAP_DEFINE(my_heap, 65536);

static void print_banner(bool start) {
  const char *marker = start ? "[START]" : "[END]";
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("%s %s - %s\n", marker, OS_NAME, TEST_NAME);
}

static void dump_snapshot(const char *phase, int round) {
  printk("[SNAPSHOT %s round=%d START]\n", phase, round);
  sys_heap_print_info(&my_heap.heap, true);
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("[SNAPSHOT %s round=%d END]\n", phase, round);
}

void main(void) {
  void *buf[BURST_COUNT];
  int round;

  print_banner(true);

  for (round = 1; round <= BURST_ROUNDS; round++) {
    int i;

    for (i = 0; i < BURST_COUNT; i++) {
      buf[i] = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
      if (!buf[i]) {
        printk("[NULL] alloc failed at round=%d, i=%d\n", round, i);
        break;
      }
    }

    dump_snapshot("AFTER_ALLOC", round);

    for (int j = 0; j < i; j++) {
      k_heap_free(&my_heap, buf[j]);
    }

    dump_snapshot("AFTER_FREE", round);

    if (i < BURST_COUNT) {
      break;
    }
  }

  print_banner(false);
}
