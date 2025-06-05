#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h> // For malloc, free
#include <stdio.h>  // For snprintf
#include <string.h> // For memset (optional, but good practice)

#define ALLOC_SIZE 48
#define GREETING_PREFIX "dynamically allocated memory"

int main(void)
{
  char *dynamic_message_buffer;

  printk("Newlib-nano (in Zephyr) Memory Demo - Concise\n\n");

  // Allocate memory using newlib's malloc
  dynamic_message_buffer = (char *)malloc(ALLOC_SIZE);

  if (dynamic_message_buffer != NULL)
  {
    // Initialize memory (optional, good practice)
    memset(dynamic_message_buffer, 0, ALLOC_SIZE);

    // Construct the message in the allocated buffer
    snprintf(dynamic_message_buffer, ALLOC_SIZE, "%s with newlib-nano", GREETING_PREFIX);

    // Print the message
    printk("Hello from %s\n", dynamic_message_buffer);

    // Free the allocated memory
    free(dynamic_message_buffer);
    dynamic_message_buffer = NULL;
    printk("Memory freed.\n");
  }
  else
  {
    printk("malloc failed to allocate %d bytes.\n", ALLOC_SIZE);
  }

  printk("\nDemo finished.\n");
  return 0;
}