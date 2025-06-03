#include "ztimer.h"
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ALLOCATOR_NAME "RIOT_TLSF"
#define TEST_NAME "LeakExhaust"
#define BLOCK_SIZE 128U

#define P_META() printf("META,tick_hz,%u\r\n", (unsigned)1000000U)

#define P_TIME(ph, op, sz, ti, to, res)                                        \
  printf("TIME,%s,%s,%u,%u,%u,%s,%lu,%lu\r\n", ph, op, (unsigned)(sz),         \
         (unsigned)(ti), (unsigned)(to), res, alloc_cnt, free_cnt)

#define P_SNAP(ph, fb, ab, mb)                                                 \
  printf("SNAP,%s,%d,%d,%u\r\n", ph, fb, ab, (unsigned)(mb))

#define P_FAULT(res)                                                           \
  printf("FAULT,%u,0xDEAD,%s\r\n", (unsigned)ztimer_now(ZTIMER_USEC), res)

static uint32_t alloc_cnt = 0;
static uint32_t free_cnt = 0;
static size_t max_live_bytes = 0;

static void emit_snapshot(const char *phase) {
  struct mallinfo mi = mallinfo();
  if ((size_t)mi.uordblks > max_live_bytes) {
    max_live_bytes = mi.uordblks;
  }
  P_SNAP(phase, mi.fordblks, mi.uordblks, max_live_bytes);
}

int main(void) {
  void *p;
  printf("# %s %s start\n", ALLOCATOR_NAME, TEST_NAME);
  P_META();

  while (1) {
    uint32_t t_in = ztimer_now(ZTIMER_USEC);
    p = malloc(BLOCK_SIZE);
    uint32_t t_out = ztimer_now(ZTIMER_USEC);

    if (!p) {
      P_TIME("leak", "malloc", BLOCK_SIZE, t_in, t_out, "NULL");
      P_FAULT("OOM");
      break;
    }
    alloc_cnt++;
    P_TIME("leak", "malloc", BLOCK_SIZE, t_in, t_out, "OK");
  }

  emit_snapshot("after_exhaust");
  printf("# %s %s end\n", ALLOCATOR_NAME, TEST_NAME);
  return 0;
}
