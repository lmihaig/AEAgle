#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#define OS_NAME "Zephyr OS"
#define TEST_NAME "FakeFree"
#define BLOCK_SIZE 128
/* offset into the middle of our BLOCK_SIZE chunk */
#define OFFSET (BLOCK_SIZE / 2)

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
  void *fake_ptr;

  print_banner(true);

  p = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
  if (p == NULL) {
    printk("[ERROR] malloc returned NULL; skipping fake free\n");
  } else {
    dump_snapshot("AFTER_MALLOC");

    fake_ptr = (uint8_t *)p + OFFSET;
    printk("[ACTION] Performing fake free at %p (p + %d)\n", fake_ptr, OFFSET);
    k_heap_free(&my_heap, fake_ptr);

    dump_snapshot("AFTER_FAKE_FREE");
  }

  print_banner(false);
}
