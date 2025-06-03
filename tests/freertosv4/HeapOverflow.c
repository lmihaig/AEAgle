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

#define ALLOCATOR_NAME "FreeRTOS-v4"
#define TEST_NAME "HeapOverflow"
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

static void HeapOverflowTest(void *pvParameters) {
  (void)pvParameters;
  void *A = NULL, *B = NULL;
  char *overflow_ptr;
  TickType_t t_in, t_out;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  emit_line("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  emit_line("META,tick_hz,%u\r\n", (unsigned)configTICK_RATE_HZ);

  t_in = xTaskGetTickCount();
  A = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  if (A == NULL) {
    emit_line("TIME,allocA,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    emit_line("FAULT,%lu,0xDEAD,OOM\r\n", (unsigned long)xTaskGetTickCount());
    goto done;
  }
  alloc_cnt++;
  emit_line("TIME,allocA,malloc,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  t_in = xTaskGetTickCount();
  B = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  if (B == NULL) {
    emit_line("TIME,allocB,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    emit_line("FAULT,%lu,0xDEAD,OOM\r\n", (unsigned long)xTaskGetTickCount());
    goto freeA;
  }
  alloc_cnt++;
  emit_line("TIME,allocB,malloc,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  emit_snapshot("after_allocs");

  overflow_ptr = (char *)A;
  t_in = xTaskGetTickCount();
  memset(overflow_ptr, 0xFF, BLOCK_SIZE + 8);
  t_out = xTaskGetTickCount();
  emit_line("TIME,overflow,memset,%u,%lu,%lu,DONE,%u,%u\r\n", BLOCK_SIZE + 8,
            (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);

  t_in = xTaskGetTickCount();
  void *C = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  if (C == NULL) {
    emit_line("TIME,allocC,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    emit_line("FAULT,%lu,0xDEAD,OOM/CORRUPT\r\n",
              (unsigned long)xTaskGetTickCount());
  } else {
    alloc_cnt++;
    emit_line("TIME,allocC,malloc,%u,%lu,%lu,OK?,%u,%u\r\n", BLOCK_SIZE,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    vPortFree(C);
    free_cnt++;
  }

  emit_snapshot("after_test");

  vPortFree(B);
  free_cnt++;
freeA:
  vPortFree(A);
  free_cnt++;
  emit_snapshot("after_cleanup");

done:
  emit_line("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  vTaskSuspend(NULL);
}

int main(void) {
  Board_init();

  xTaskCreate(HeapOverflowTest, "HeapOverflowTest", 512, NULL, 1, NULL);

  vTaskStartScheduler();

  for (;;)
    ;
  return 0;
}
