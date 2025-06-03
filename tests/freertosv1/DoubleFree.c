#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include "ti_drivers_config.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/drivers/Board.h>
#include <ti/drivers/UART2.h>

#define ALLOCATOR_NAME "FreeRTOS-v1"
#define TEST_NAME "DoubleFree"
#define BLOCK_SIZE 128U

static uint32_t alloc_cnt = 0, free_cnt = 0;

static UART2_Handle uart;
static UART2_Params uartParams;

static void emit_line(const char *fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0) {
    UART2_write(uart, buf, len, NULL);
  }
}

static void emit_snapshot(const char *phase) {
  size_t free_now = xPortGetFreeHeapSize();
  size_t min_free = xPortGetMinimumEverFreeHeapSize();
  size_t total = (size_t)configTOTAL_HEAP_SIZE;
  size_t used_now = total - free_now;
  size_t used_max = total - min_free;

  emit_line("SNAP,%s,%lu,%lu,%lu\r\n", phase, (unsigned long)free_now,
            (unsigned long)used_now, (unsigned long)used_max);
}

static void DoubleFreeTest(void *pvParameters) {
  (void)pvParameters;
  void *p = NULL;
  TickType_t t_in, t_out;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  emit_line("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  emit_line("META,tick_hz,%u\r\n", (unsigned)configTICK_RATE_HZ);

  t_in = xTaskGetTickCount();
  p = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();

  if (p == NULL) {
    emit_line("TIME,setup,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    emit_line("FAULT,%lu,0xDEAD,OOM\r\n", (unsigned long)xTaskGetTickCount());
    goto done;
  }
  alloc_cnt++;
  emit_line("TIME,setup,malloc,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  emit_snapshot("after_alloc");

  t_in = xTaskGetTickCount();
  vPortFree(p);
  t_out = xTaskGetTickCount();

  free_cnt++;
  emit_line("TIME,free1,free,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  emit_snapshot("after_free1");

  t_in = xTaskGetTickCount();
  vPortFree(p);
  t_out = xTaskGetTickCount();

  free_cnt++;
  emit_line("TIME,free2,free,%u,%lu,%lu,dblfree,%u,%u\r\n", BLOCK_SIZE,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  emit_snapshot("after_free2");

done:
  emit_line("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  vTaskSuspend(NULL);
}

int main(void) {
  Board_init();

  xTaskCreate(DoubleFreeTest, "DoubleFreeTest", 512, NULL, 1, NULL);

  vTaskStartScheduler();

  for (;;)
    ;
  return 0;
}
