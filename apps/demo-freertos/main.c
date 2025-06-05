#include "FreeRTOS.h"
#include "task.h"
#include "portable.h"          // For pvPortMalloc, vPortFree
#include "ti_drivers_config.h" // For Board_init, UART2_open etc.
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ti/drivers/Board.h>
#include <ti/drivers/UART2.h>

#define ALLOC_SIZE 41
#define GREETING_PREFIX "dynamically allocated memory"

static UART2_Handle uart;
static UART2_Params uartParams;

// Simple UART print function
static void app_printf(const char *fmt, ...)
{
  char buf[128];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len > 0 && uart)
  {
    UART2_write(uart, buf, len, NULL);
  }
}

static void helloMemoryTask(void *pvParameters)
{
  (void)pvParameters;
  char *dynamic_message_buffer;

  // Initialize UART
  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  if (uart == NULL)
  {
    // If UART fails, there's no way to print, so just loop or suspend
    for (;;)
      ;
  }

  app_printf("FreeRTOS Memory Demo - Concise\r\n\r\n");

  // Allocate memory using pvPortMalloc
  dynamic_message_buffer = (char *)pvPortMalloc(ALLOC_SIZE);

  if (dynamic_message_buffer != NULL)
  {
    // Construct the message in the allocated buffer
    snprintf(dynamic_message_buffer, ALLOC_SIZE, "%s in FreeRTOS", GREETING_PREFIX);

    // Print the message
    app_printf("Hello from %s\r\n", dynamic_message_buffer);

    // Free the allocated memory
    vPortFree(dynamic_message_buffer);
    dynamic_message_buffer = NULL;
    app_printf("Memory freed.\r\n");
  }
  else
  {
    app_printf("pvPortMalloc failed to allocate %d bytes.\r\n", ALLOC_SIZE);
  }

  app_printf("\r\nDemo finished.\r\n");

  // Clean up UART and suspend task
  if (uart)
  {
    UART2_close(uart);
    uart = NULL;
  }
  vTaskSuspend(NULL); // Suspend this task
}

int main(void)
{
  Board_init(); // Initialize board-specific stuff

  // Create the task
  xTaskCreate(
      helloMemoryTask,                // Function that implements the task.
      "HelloMemTask",                 // Text name for the task.
      configMINIMAL_STACK_SIZE + 100, // Stack size in words, not bytes.
      NULL,                           // Parameter passed into the task.
      1,                              // Priority at which the task is created.
      NULL                            // Used to pass out the created task's handle.
  );

  // Start the scheduler.
  vTaskStartScheduler();

  // Should not reach here as the scheduler is running.
  for (;;)
    ;
  return 0;
}
