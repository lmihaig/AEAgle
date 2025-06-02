#include "FreeRTOS.h"
#include "task.h"
#include <semaphore.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ti/drivers/Board.h>
#include <ti/drivers/UART2.h>

#include "ti_drivers_config.h"

static void vHelloTask(void *pvParams) {
  UART2_Handle uart;
  UART2_Params uartParams;

  UART2_Params_init(&uartParams);
  uartParams.baudRate = 115200;

  void *mem = pvPortMalloc(128);
  char msg[40];
  uart = UART2_open(CONFIG_UART2_0, &uartParams);

  snprintf(msg, sizeof(msg), "OOPS? @ %p\r\n", mem);
  UART2_write(uart, msg, strlen(msg), NULL);
  vPortFree(mem);
  vTaskDelete(NULL);
}

int main(void) {
  Board_init();
  xTaskCreate(vHelloTask, "HELLO", 256, NULL, 1, NULL);
  vTaskStartScheduler();
  for (;;)
    ;
}
