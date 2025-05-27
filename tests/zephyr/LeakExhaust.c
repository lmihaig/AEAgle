#include <zephyr/kernel.h>
#include <zephyr/kernel_includes.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/sys_heap.h>

#define OS_NAME "Zephyr OS"
#define TEST_NAME "LeakExhaust"
#define BLOCK_SIZE 128
#define MAX_ALLOCS 2048

K_HEAP_DEFINE(my_heap, 65536);

static void print_banner(bool start) {
  const char *marker = start ? "[WORK WORK]" : "[JOB'S DONE]";
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("%s %s - %s\n", marker, OS_NAME, TEST_NAME);
}

static void dump_snapshot() {
  printk("[SNAPSHOT START]\n");
  sys_heap_print_info(&my_heap.heap, true);
  printk("[TICKS] %llu\n", k_uptime_ticks());
  printk("[SNAPSHOT END]\n");
}

int main(void) {
  int count = 0;

  print_banner(true);

  while (count < MAX_ALLOCS) {
    void *p = k_heap_alloc(&my_heap, BLOCK_SIZE, K_NO_WAIT);
    if (!p) {
      printk("[NULL] Allocation failed at count=%d\n", count);
      break;
    }
    count++;
    dump_snapshot();
  }

  print_banner(false);
}
