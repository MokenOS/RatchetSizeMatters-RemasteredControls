TARGET = ratchet_remastered
OBJS = src/main.o

CFLAGS = -O2 -G0 -Wall -Wextra -fno-pic
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = src/exports.exp

USE_KERNEL_LIBS = 1
LDFLAGS = -nostartfiles
LIBS = -lpspsystemctrl_kernel

PSP_FW_VERSION = 660

PSPSDK := $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
