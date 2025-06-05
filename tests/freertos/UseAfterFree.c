#include "FreeRTOS.h"
#include "portable.h"
#include "task.h"
#include "ti_drivers_config.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ti/drivers/Board.h>
#include <ti/drivers/UART2.h>

#ifndef ALLOCATOR_NAME
#define ALLOCATOR_NAME "FreeRTOS"
#endif

#define TEST_NAME "UseAfterFree"
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

#define LOG_LEAK_FREERTOS(addr_val) \
  emit_line("LEAK,%p\r\n", (void *)(addr_val))

#define LOG_NOLEAK_FREERTOS(addr_val) \
  emit_line("NOLEAK,%p\r\n", (void *)(addr_val))

static uint32_t alloc_cnt = 0, free_cnt = 0;
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

static void UseAfterFreeTest(void *pvParameters)
{
  (void)pvParameters;
  void *p1 = NULL, *p2 = NULL;
  uint8_t *buf1, *buf2;
  TickType_t t_in, t_out, t_inspect_in, t_inspect_out, t_cleanup_in, t_cleanup_out;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  LOG_TEST_START(ALLOCATOR_NAME, TEST_NAME);
  LOG_META_FREERTOS(configTICK_RATE_HZ);

  emit_snapshot("baseline");

  t_in = xTaskGetTickCount();
  p1 = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  if (p1 == NULL)
  {
    LOG_TIME_FREERTOS("setup", "malloc", BLOCK_SIZE, t_in, t_out, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_FREERTOS(xTaskGetTickCount(), "OOM");
    goto done;
  }
  alloc_cnt++;
  LOG_TIME_FREERTOS("setup", "malloc", BLOCK_SIZE, t_in, t_out, "OK", alloc_cnt, free_cnt);
  memset(p1, 0x5A, BLOCK_SIZE);
  emit_snapshot("after_setup");

  t_in = xTaskGetTickCount();
  vPortFree(p1);
  t_out = xTaskGetTickCount();
  free_cnt++;
  LOG_TIME_FREERTOS("setup", "free", BLOCK_SIZE, t_in, t_out, "OK", alloc_cnt, free_cnt);
  emit_snapshot("after_free1");

  buf1 = (uint8_t *)p1;
  t_in = xTaskGetTickCount();
  memset(buf1, 0xA5, BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  LOG_TIME_FREERTOS("uaf_write", "memset_uaf", BLOCK_SIZE, t_in, t_out, "UAF_WRITE_DONE", alloc_cnt, free_cnt);
  emit_snapshot("after_uaf_write");

  t_in = xTaskGetTickCount();
  p2 = pvPortMalloc(BLOCK_SIZE);
  t_out = xTaskGetTickCount();
  if (p2 == NULL)
  {
    LOG_TIME_FREERTOS("uaf_realloc", "malloc", BLOCK_SIZE, t_in, t_out, "NULL", alloc_cnt, free_cnt);
    LOG_FAULT_FREERTOS(xTaskGetTickCount(), "OOM");
    goto done;
  }
  alloc_cnt++;
  LOG_TIME_FREERTOS("uaf_realloc", "malloc", BLOCK_SIZE, t_in, t_out, "OK", alloc_cnt, free_cnt);

  t_inspect_in = xTaskGetTickCount();
  bool leaked = false;
  buf2 = (uint8_t *)p2;
  for (int i = 0; i < BLOCK_SIZE; ++i)
  {
    if (buf2[i] == 0xA5)
    {
      leaked = true;
      break;
    }
  }
  t_inspect_out = xTaskGetTickCount();

  if (leaked)
  {
    LOG_TIME_FREERTOS("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "LEAK_DETECTED", alloc_cnt, free_cnt);
    LOG_LEAK_FREERTOS(p2);
  }
  else
  {
    LOG_TIME_FREERTOS("uaf_inspect", "inspect_uaf", BLOCK_SIZE, t_inspect_in, t_inspect_out, "NO_LEAK_DETECTED", alloc_cnt, free_cnt);
    LOG_NOLEAK_FREERTOS(p2);
  }
  emit_snapshot("post_primitive_realloc");

  if (p2 != NULL)
  {
    t_cleanup_in = xTaskGetTickCount();
    vPortFree(p2);
    p2 = NULL;
    t_cleanup_out = xTaskGetTickCount();
    free_cnt++;
    LOG_TIME_FREERTOS("cleanup", "free", BLOCK_SIZE, t_cleanup_in, t_cleanup_out, "OK", alloc_cnt, free_cnt);
  }
  emit_snapshot("post_cleanup");

done:
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
  xTaskCreate(UseAfterFreeTest, TEST_NAME, 512, NULL, 1, NULL);
  vTaskStartScheduler();
  for (;;)
    ;
  return 0;
}