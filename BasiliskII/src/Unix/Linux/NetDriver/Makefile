# Linux makefile for sheep_net driver
KERNEL_DIR	= /lib/modules/$(shell uname -r)
KERNEL_SOURCE	= $(KERNEL_DIR)/build
LV		:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo 24 || echo 26)
MP		:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo "o" || echo "ko")

ifeq ($(LV),26)
# Kernel 2.6

obj-m = sheep_net.o

sheep_net.ko: sheep_net.c
	$(MAKE) -C $(KERNEL_SOURCE) M=$$PWD modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$$PWD clean

else
# Kernel 2.4

## System specific configuration
CPPFLAGS = -I. -I$(KERNEL_SOURCE)/include
CFLAGS = -O2 -Wall -D__KERNEL__ -DMODULE -D_LOOSE_KERNEL_NAMES
ASFLAGS =
LDFLAGS =
LIBS =

## Files
OBJS = sheep_net.o

## Rules
sheep_net.o: sheep_net.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) sheep_net.c

clean:
	-rm $(OBJS)

dep depend:
	makedepend $(CPPFLAGS) -Y *.c

endif

dev:
	mknod /dev/sheep_net c 10 198

install: sheep_net.$(MP)
	install -d $(KERNEL_DIR)/misc
	install -m 644 sheep_net.$(MP) $(KERNEL_DIR)/misc
	depmod -a

# DO NOT DELETE
