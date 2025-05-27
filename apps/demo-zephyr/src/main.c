#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#define OS_NAME "Zephyr OS"
#define TEST_NAME "UseAfterFree"
#define BLOCK_SIZE 128
#define PATTERN_BYTE 0xAA
#define INSPECT_BYTES 8

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

static void inspect_data(uint8_t *ptr, const char *label) {
  printk("[INSPECT %s] first %d bytes:", label, INSPECT_BYTES);
  for (int i = 0; i < INSPECT_BYTES; i++) {
    printk(" %02X", ptr[i]);
  }
  printk("\n");
}

void main(void) {
  void *p1 = NULL;
  void *p2 = NULL;

  print_banner(true);

  p1 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
  if (!p1) {
    printk("[ERROR] malloc returned NULL; aborting test\n");
    print_banner(false);
    return;
  }
  dump_snapshot("AFTER_MALLOC1");

  k_heap_free(&my_heap, p1);
  dump_snapshot("AFTER_FREE1");

  memset(p1, PATTERN_BYTE, BLOCK_SIZE);
  printk("[ACTION] Wrote pattern 0x%X into freed memory at %p\n", PATTERN_BYTE,
         p1);
  dump_snapshot("AFTER_UAF_WRITE");

  p2 = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
  if (!p2) {
    printk("[ERROR] second malloc returned NULL; cannot inspect data\n");
    print_banner(false);
    return;
  }
  dump_snapshot("AFTER_MALLOC2");

  inspect_data((uint8_t *)p2, "NEW_BLOCK");

  print_banner(false);
}
