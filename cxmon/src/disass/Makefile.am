## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libdisass.a
libdisass_a_SOURCES = ansidecl.h bfd.h dis-asm.h floatformat.c floatformat.h \
  i386-dis.c m68k-dis.c m68k-opc.c m68k.h opintl.h

# Extra includes, for <sysdeps.h>
CPPFLAGS = -I$(srcdir)/..
