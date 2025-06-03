APPLICATION = demo-riot
BOARD ?= native
RIOTBASE ?= $(CURDIR)/../../operating-systems/RIOT

SRC += $(CURDIR)/src/main.c

USEMODULE += ztimer_usec
USEMODULE += malloc_monitor
USEMODULE += memarray

CFLAGS += -Wno-error=free-nonheap-object -Wno-free-nonheap-object
CFLAGS += -DCONFIG_MODULE_SYS_MALLOC_MONITOR_VERBOSE=0
CFLAGS += -Wno-error=use-after-free

include $(RIOTBASE)/Makefile.include
