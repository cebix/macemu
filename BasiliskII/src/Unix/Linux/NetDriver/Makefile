# Linux makefile for sheep_net driver

## System specific configuration
CC = cc
CPPFLAGS = -I. -I/usr/src/linux/include
CFLAGS = -O2 -Wall -D__KERNEL__ -DMODULE -D_LOOSE_KERNEL_NAMES
ASFLAGS =
LDFLAGS =
LIBS =
KERNDIR = /lib/modules/$(shell uname -r)

## Files
OBJS = sheep_net.o

## Rules
sheep_net.o: sheep_net.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) sheep_net.c

dev:
	mknod /dev/sheep_net c 10 198

install: sheep_net.o
	install -d $(KERNDIR)/misc
	install -m 644 sheep_net.o $(KERNDIR)/misc
	depmod -a

clean:
	-rm $(OBJS)

dep depend:
	makedepend $(CPPFLAGS) -Y *.c

# DO NOT DELETE
