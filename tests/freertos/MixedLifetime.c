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

#ifndef ALLOCATOR_NAME
#define ALLOCATOR_NAME "FreeRTOS"
#endif

#define TEST_NAME "MixedLifetime"
#define BLOCK_SIZE 128U
#define PIN_COUNT 5
#define BURST_ROUNDS 10
#define BURST_COUNT 10

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

static size_t g_min_free_ever = (size_t)-1;

static void emit_snapshot(const char *phase) {
  size_t free_now = xPortGetFreeHeapSize();
  size_t total = (size_t)configTOTAL_HEAP_SIZE;

  if (free_now < g_min_free_ever) {
    g_min_free_ever = free_now;
  }

  size_t used_now = total - free_now;
  size_t used_max = total - g_min_free_ever;

  emit_line("SNAP,%s,%lu,%lu,%lu\r\n", phase, (unsigned long)free_now,
            (unsigned long)used_now, (unsigned long)used_max);
}

static void MixedLifetimeTest(void *pvParameters) {
  (void)pvParameters;
  void *pinned[PIN_COUNT];
  void *buf[BURST_COUNT];
  TickType_t t_in, t_out;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  emit_line("# %s %s start\r\n", ALLOCATOR_NAME, TEST_NAME);
  emit_line("META,tick_hz,%u\r\n", (unsigned)configTICK_RATE_HZ);

  for (int i = 0; i < PIN_COUNT; ++i) {
    t_in = xTaskGetTickCount();
    pinned[i] = pvPortMalloc(BLOCK_SIZE * 2);
    t_out = xTaskGetTickCount();
    if (pinned[i] == NULL) {
      emit_line("TIME,pin,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE * 2,
                (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
      emit_line("FAULT,%lu,0xDEAD,OOM\r\n", (unsigned long)xTaskGetTickCount());
      goto done;
    }
    alloc_cnt++;
    emit_line("TIME,pin,malloc,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE * 2,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
  }
  emit_snapshot("after_pins");

  for (int round = 1; round <= BURST_ROUNDS; ++round) {
    int i;
    for (i = 0; i < BURST_COUNT; ++i) {
      t_in = xTaskGetTickCount();
      buf[i] = pvPortMalloc(BLOCK_SIZE);
      t_out = xTaskGetTickCount();
      if (buf[i] == NULL) {
        emit_line("TIME,burst,malloc,%u,%lu,%lu,NULL,%u,%u\r\n", BLOCK_SIZE,
                  (unsigned long)t_in, (unsigned long)t_out, alloc_cnt,
                  free_cnt);
        emit_line("FAULT,%lu,0xDEAD,OOM\r\n",
                  (unsigned long)xTaskGetTickCount());
        goto cleanup;
      }
      alloc_cnt++;
      emit_line("TIME,burst,malloc,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
                (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    }
    emit_snapshot("after_burst_alloc");

    for (int j = i - 1; j >= 0; --j) {
      t_in = xTaskGetTickCount();
      vPortFree(buf[j]);
      t_out = xTaskGetTickCount();
      free_cnt++;
      emit_line("TIME,burst,free,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE,
                (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
    }
    emit_snapshot("after_burst_free");
  }

cleanup:
  for (int i = 0; i < PIN_COUNT; ++i) {
    t_in = xTaskGetTickCount();
    vPortFree(pinned[i]);
    t_out = xTaskGetTickCount();
    free_cnt++;
    emit_line("TIME,cleanup,free,%u,%lu,%lu,OK,%u,%u\r\n", BLOCK_SIZE * 2,
              (unsigned long)t_in, (unsigned long)t_out, alloc_cnt, free_cnt);
  }
  emit_snapshot("after_cleanup");

done:
  emit_line("# %s %s end\r\n", ALLOCATOR_NAME, TEST_NAME);
  vTaskSuspend(NULL);
}

int main(void) {
  Board_init();

  xTaskCreate(MixedLifetimeTest, "MixedLifetimeTest", 512, NULL, 1, NULL);

  vTaskStartScheduler();

  for (;;)
    ;
  return 0;
}
