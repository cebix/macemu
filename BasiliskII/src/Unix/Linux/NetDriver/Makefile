# Linux makefile for SheepShaver

## System specific configuration
CC = cc
CPPFLAGS = -I. -I/usr/src/linux/include
CFLAGS = -O2 -Wall -fno-builtin -D__KERNEL__ -DMODULE
ASFLAGS =
LDFLAGS =
LIBS =

## Files
OBJS = sheep_net.o

## Rules
sheep_net.o: sheep_net.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) sheep_net.c

dev:
	mknod /dev/sheep_net c 10 198

clean:
	-rm $(OBJS)

dep depend:
	makedepend $(CPPFLAGS) -Y *.c

# DO NOT DELETE
