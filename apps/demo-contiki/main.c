#include "contiki.h"
#include "lib/heapmem.h"
#include "lib/memb.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Define for memb
#define MEMB_BLOCK_SIZE 32
#define MEMB_NUM_BLOCKS 1
struct my_memb_block
{
  char message[MEMB_BLOCK_SIZE];
};
MEMB(my_memb_pool, struct my_memb_block, MEMB_NUM_BLOCKS);

// Define for heapmem
#define HEAPMEM_ALLOC_SIZE 40

PROCESS(hello_mem_process, "Hello Memory Process");
AUTOSTART_PROCESSES(&hello_mem_process);

PROCESS_THREAD(hello_mem_process, ev, data)
{
  static struct my_memb_block *memb_ptr;
  static char *heapmem_ptr;

  PROCESS_BEGIN();

  printf("Contiki Memory Demo - Concise\r\n\r\n");

  // --- MEMB Demo (Concise) ---
  memb_init(&my_memb_pool);
  memb_ptr = (struct my_memb_block *)memb_alloc(&my_memb_pool);

  if (memb_ptr != NULL)
  {
    snprintf(memb_ptr->message, MEMB_BLOCK_SIZE, "memb block");
    printf("Hello from %s (memb)\r\n", memb_ptr->message);
    memb_free(&my_memb_pool, memb_ptr);
  }
  else
  {
    printf("Memb allocation failed.\r\n");
  }

  // --- HEAPMEM Demo (Concise) ---
  heapmem_ptr = (char *)heapmem_alloc(HEAPMEM_ALLOC_SIZE);

  if (heapmem_ptr != NULL)
  {
    snprintf(heapmem_ptr, HEAPMEM_ALLOC_SIZE, "heapmem block");
    printf("Hello from %s (heapmem)\r\n", heapmem_ptr);
    heapmem_free(heapmem_ptr);
  }
  else
  {
    printf("Heapmem allocation failed.\r\n");
  }

  printf("\r\nDemo finished.\r\n");

  PROCESS_END();
}
