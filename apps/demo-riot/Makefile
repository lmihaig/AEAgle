APPLICATION = demo-riot
BOARD ?= native
RIOTBASE ?= $(CURDIR)/../../operating-systems/RIOT

SRC += $(CURDIR)/src/main.c

USEMODULE += ztimer_usec
USEMODULE += malloc_monitor
USEMODULE += memarray

# CFLAGS += -Wno-error=free-nonheap-object -Wno-free-nonheap-object
# CFLAGS += -Wno-error=use-after-free
CFLAGS += -DCONFIG_MODULE_SYS_MALLOC_MONITOR_VERBOSE=0

include $(RIOTBASE)/Makefile.include
