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

#define TEST_NAME "LeakExhaust"
#define BLOCK_SIZE 128U

static UART2_Handle uart;
static UART2_Params uartParams;

static void emit_line(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0)
  {
    UART2_write(uart, buf, len, NULL);
  }
}

#define LOG_TEST_START(alloc_name, test_name_str) \
  emit_line("# %s %s start\r\n", (alloc_name), (test_name_str))

#define LOG_TEST_END(alloc_name, test_name_str) \
  emit_line("# %s %s end\r\n", (alloc_name), (test_name_str))

#define LOG_META_FREERTOS(tick_hz_val) \
  emit_line("META,tick_hz,%u\r\n", (unsigned)(tick_hz_val))

#define LOG_TIME_FREERTOS(phase_str, op_str, size_val, time_in, time_out, result_str, ac, fc) \
  emit_line("TIME,%s,%s,%u,%lu,%lu,%s,%u,%u\r\n",                                             \
            (phase_str), (op_str), (unsigned)(size_val),                                      \
            (unsigned long)(time_in), (unsigned long)(time_out), (result_str),                \
            (unsigned int)(ac), (unsigned int)(fc))

#define LOG_SNAP_FREERTOS(phase_str, free_b_val, allocated_b_val, max_alloc_b_val) \
  emit_line("SNAP,%s,%lu,%lu,%lu\r\n",                                             \
            (phase_str), (unsigned long)(free_b_val),                              \
            (unsigned long)(allocated_b_val), (unsigned long)(max_alloc_b_val))

#define LOG_FAULT_FREERTOS(current_ticks, error_str) \
  emit_line("FAULT,%lu,0xDEAD,%s\r\n", (unsigned long)(current_ticks), (error_str))

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;
static size_t g_min_free_ever = (size_t)-1;

static void emit_snapshot(const char *phase)
{
  size_t free_now = xPortGetFreeHeapSize();
  size_t total = (size_t)configTOTAL_HEAP_SIZE;

  if (free_now < g_min_free_ever)
  {
    g_min_free_ever = free_now;
  }

  size_t used_now = total - free_now;
  size_t used_max = total - g_min_free_ever;

  LOG_SNAP_FREERTOS(phase, free_now, used_now, used_max);
}

static void LeakExhaustTest(void *pvParameters)
{
  (void)pvParameters;
  void *p = NULL;
  TickType_t t_in, t_out;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_FREERTOS(configTICK_RATE_HZ);

  emit_snapshot("baseline");

  while (1)
  {
    t_in = xTaskGetTickCount();
    p = pvPortMalloc(BLOCK_SIZE);
    t_out = xTaskGetTickCount();

    if (p == NULL)
    {
      LOG_TIME_FREERTOS("leakloop", "malloc", BLOCK_SIZE, t_in, t_out, "NULL", alloc_cnt, free_cnt);
      LOG_FAULT_FREERTOS(xTaskGetTickCount(), "OOM");
      break;
    }
    alloc_cnt++;
    LOG_TIME_FREERTOS("leakloop", "malloc", BLOCK_SIZE, t_in, t_out, "OK", alloc_cnt, free_cnt);
  }

  emit_snapshot("after_leakloop_exhaustion");
  LOG_TEST_END(ALLOCATOR_NAME, TEST_NAME);
  if (uart)
  {
    UART2_close(uart);
  }
  vTaskSuspend(NULL);
}

int main(void)
{
  Board_init();

  xTaskCreate(LeakExhaustTest, TEST_NAME, 512, NULL, 1, NULL);

  vTaskStartScheduler();

  for (;;)
    ;
  return 0;
}