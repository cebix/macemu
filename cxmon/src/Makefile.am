## Process this file with automake to produce Makefile.in

SUBDIRS = disass

bin_PROGRAMS = cxmon
cxmon_SOURCES = main.cpp mon.cpp mon.h mon_6502.cpp mon_z80.cpp mon_atraps.h \
  mon_cmd.cpp mon_cmd.h mon_disass.cpp mon_disass.h mon_lowmem.cpp mon_lowmem.h \
  mon_ppc.cpp sysdeps.h

cxmon_LDADD = disass/libdisass.a
