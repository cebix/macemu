# Makefile for creating SheepShaver distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION := 2.3
VERNAME := SheepShaver-$(VERSION)
CVSDATE := $(shell date "+%Y%m%d")

SRCARCHIVE := $(VERNAME)-$(CVSDATE).tar.gz

TMPDIR := $(shell date +/tmp/build%m%s)
ISODATE := $(shell date "+%Y-%m-%d %H:%M")
DOCS := NEWS
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
	cd $(TMPDIR); cvs export -D "$(ISODATE)" BasiliskII SheepShaver
	cd $(TMPDIR)/SheepShaver/src/Unix && mkdir Darwin
	cd $(TMPDIR)/SheepShaver && make links
	cd $(TMPDIR)/SheepShaver/src/Unix && NO_CONFIGURE=1 ./autogen.sh
	cd $(TMPDIR)/SheepShaver/src/Windows && NO_CONFIGURE=1 ../Unix/autogen.sh
	rm $(TMPDIR)/SheepShaver/Makefile
	cp -aL $(TMPDIR)/SheepShaver $(TMPDIR)/$(VERNAME)
	cd $(TMPDIR); tar cfz $@ $(VERNAME)
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# Links to Basilisk II sources
#
links:
	@list='adb.cpp audio.cpp cdrom.cpp disk.cpp extfs.cpp pict.c \
	       prefs.cpp scsi.cpp sony.cpp xpram.cpp \
	       include/adb.h include/audio.h include/audio_defs.h \
	       include/cdrom.h include/clip.h include/debug.h include/disk.h \
	       include/extfs.h include/extfs_defs.h include/pict.h \
	       include/prefs.h include/scsi.h include/serial.h \
	       include/serial_defs.h include/sony.h include/sys.h \
	       include/timer.h include/xpram.h \
	       BeOS/audio_beos.cpp BeOS/extfs_beos.cpp BeOS/scsi_beos.cpp \
	       BeOS/serial_beos.cpp BeOS/sys_beos.cpp BeOS/timer_beos.cpp \
	       BeOS/xpram_beos.cpp BeOS/SheepDriver BeOS/SheepNet \
	       CrossPlatform/sigsegv.h CrossPlatform/sigsegv.cpp CrossPlatform/vm_alloc.h CrossPlatform/vm_alloc.cpp \
               CrossPlatform/video_vosf.h CrossPlatform/video_blit.h CrossPlatform/video_blit.cpp \
	       Unix/audio_oss_esd.cpp Unix/bincue_unix.cpp Unix/bincue_unix.h \
	       Unix/vhd_unix.cpp \
	       Unix/extfs_unix.cpp Unix/serial_unix.cpp \
	       Unix/sshpty.h Unix/sshpty.c Unix/strlcpy.h Unix/strlcpy.c \
	       Unix/sys_unix.cpp Unix/timer_unix.cpp Unix/xpram_unix.cpp \
	       Unix/semaphore.h Unix/posix_sem.cpp Unix/config.sub Unix/config.guess Unix/m4 \
	       Unix/keycodes Unix/tunconfig Unix/clip_unix.cpp Unix/Irix/audio_irix.cpp \
	       Unix/Linux/scsi_linux.cpp Unix/Linux/NetDriver Unix/ether_unix.cpp \
	       Unix/rpc.h Unix/rpc_unix.cpp Unix/ldscripts \
	       Unix/tinyxml2.h Unix/tinyxml2.cpp Unix/disk_unix.h \
	       Unix/disk_sparsebundle.cpp Unix/Darwin/mkstandalone \
	       Unix/Darwin/lowmem.c Unix/Darwin/pagezero.c Unix/Darwin/testlmem.sh \
	       dummy/audio_dummy.cpp dummy/clip_dummy.cpp dummy/serial_dummy.cpp \
	       dummy/prefs_editor_dummy.cpp dummy/scsi_dummy.cpp SDL slirp \
	       MacOSX/sys_darwin.cpp MacOSX/clip_macosx.cpp MacOSX/clip_macosx64.mm \
	       MacOSX/macos_util_macosx.h Unix/cpr.sh \
	       MacOSX/extfs_macosx.cpp Windows/clip_windows.cpp \
	       MacOSX/MacOSX_sound_if.cpp MacOSX/MacOSX_sound_if.h \
	       MacOSX/AudioBackEnd.cpp MacOSX/AudioBackEnd.h \
	       MacOSX/AudioDevice.cpp MacOSX/AudioDevice.h MacOSX/audio_macosx.cpp \
	       MacOSX/utils_macosx.mm MacOSX/utils_macosx.h \
	       Windows/cd_defs.h Windows/cdenable Windows/extfs_windows.cpp \
	       Windows/posix_emu.cpp Windows/posix_emu.h Windows/sys_windows.cpp \
	       Windows/timer_windows.cpp Windows/util_windows.cpp \
	       Windows/util_windows.h Windows/xpram_windows.cpp \
	       Windows/kernel_windows.h Windows/kernel_windows.cpp \
	       Windows/serial_windows.cpp Windows/router Windows/b2ether \
	       Windows/ether_windows.h Windows/ether_windows.cpp \
	       Windows/serial_windows.cpp Windows/prefs_editor_gtk.cpp \
	       uae_cpu/compiler/codegen_x86.h'; \
	PREFIX="../"; case $(B2_TOPDIR) in /*) PREFIX="";; esac; \
	for i in $$list; do \
	  if test "$$i" != "\\"; then \
	    echo $$i; o=$$i; \
	    case $$i in *codegen_x86.h) o=kpx_cpu/src/cpu/jit/x86/codegen_x86.h;; esac; \
	    SUB=`echo $$o | sed 's;[^/]*/;../;g' | sed 's;[^/]*$$;;'` ;\
	    ln -sf "$$PREFIX$$SUB$(B2_TOPDIR)/src/$$i" src/$$o; \
	  fi; \
	done; \
	ln -sf ../../../../../SheepShaver/src/Unix/config.h $(B2_TOPDIR)/src/Unix/Linux/NetDriver/config.h
