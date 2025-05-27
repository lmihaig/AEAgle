#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#define OS_NAME "Zephyr OS"
#define TEST_NAME "DoubleFree"
#define BLOCK_SIZE 128

K_HEAP_DEFINE(my_heap, 65536);

static void print_banner(bool start) {
  const char *marker = start ? "[START]" : "[END]";
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("%s %s - %s\n", marker, OS_NAME, TEST_NAME);
}

static void dump_snapshot(const char *phase) {
  printk("[SNAPSHOT %s START]\n", phase);
  sys_heap_print_info(&my_heap.heap, true);
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("[SNAPSHOT %s END]\n", phase);
}

void main(void) {
  void *p;

  print_banner(true);

  p = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
  if (!p) {
    printk("[ERROR] malloc returned NULL\n");
    goto done;
  }
  dump_snapshot("AFTER_MALLOC");

  k_heap_free(&my_heap, p);
  dump_snapshot("AFTER_FIRST_FREE");

  printk("[ACTION] Performing second free (double-free)\n");
  k_heap_free(&my_heap, p);
  dump_snapshot("AFTER_SECOND_FREE");

done:
  print_banner(false);
}
