# Makefile for creating SheepShaver distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION := 2
RELEASE := 2
VERNAME := SheepShaver-$(VERSION)

SRCARCHIVE := $(shell date +SheepShaver_src_%d%m%Y.tar.gz)

TMPDIR := $(shell date +/tmp/build%m%s)
DOCS := HISTORY LOG TODO
SRCS := src

# Where Basilisk II directory can be found
B2_TOPDIR := ../BasiliskII

default: help

help:
	@echo "This top-level Makefile is for creating SheepShaver distributions."
	@echo "The following targets are available:"
	@echo "  tarball  source tarball ($(SRCARCHIVE))"
	@echo "  links    create links to Basilisk II sources"

clean:
	-rm -f $(SRCARCHIVE)

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRCS) $(DOCS)
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	cd $(TMPDIR); cvs export -D "$(ISODATE)" SheepShaver
	rm $(TMPDIR)/SheepShaver/Makefile
	mv $(TMPDIR)/SheepShaver $(TMPDIR)/$(VERNAME)
	cd $(TMPDIR); tar cfz $@ $(VERNAME)
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# Links to Basilisk II sources
#
links:
	@list='adb.cpp audio.cpp cdrom.cpp disk.cpp extfs.cpp prefs.cpp \
	       scsi.cpp sony.cpp xpram.cpp \
	       include/adb.h include/audio.h include/audio_defs.h \
	       include/cdrom.h include/clip.h include/debug.h include/disk.h \
	       include/extfs.h include/extfs_defs.h include/prefs.h \
	       include/scsi.h include/serial.h include/serial_defs.h \
	       include/sony.h include/sys.h include/timer.h include/xpram.h \
	       BeOS/audio_beos.cpp BeOS/extfs_beos.cpp BeOS/scsi_beos.cpp \
	       BeOS/serial_beos.cpp BeOS/sys_beos.cpp BeOS/timer_beos.cpp \
	       BeOS/xpram_beos.cpp BeOS/SheepDriver BeOS/SheepNet \
	       Unix/audio_oss_esd.cpp Unix/extfs_unix.cpp Unix/serial_unix.cpp \
	       Unix/sshpty.h Unix/sshpty.c Unix/strlcpy.h Unix/strlcpy.c \
	       Unix/sys_unix.cpp Unix/timer_unix.cpp Unix/xpram_unix.cpp \
	       Unix/sigsegv.h Unix/sigsegv.cpp Unix/vm_alloc.h Unix/vm_alloc.cpp \
	       Unix/posix_sem.cpp Unix/video_vosf.h Unix/video_blit.h \
	       Unix/video_blit.cpp Unix/config.sub Unix/config.guess \
	       Unix/keycodes Unix/tunconfig \
	       Unix/Linux/scsi_linux.cpp Unix/Linux/NetDriver \
	       Unix/Darwin/lowmem.c Unix/Darwin/pagezero.c Unix/Darwin/testlmem.sh \
	       dummy/audio_dummy.cpp dummy/clip_dummy.cpp \
	       dummy/prefs_editor_dummy.cpp dummy/scsi_dummy.cpp SDL \
	       MacOSX/sys_darwin.cpp MacOSX/clip_macosx.cpp'; \
	PREFIX="`pwd`/"; case $(B2_TOPDIR) in /*) PREFIX="";; esac; \
	for i in $$list; do \
	  echo $$i; \
	  ln -sf $${PREFIX}$(B2_TOPDIR)/src/$$i src/$$i; \
	done;
