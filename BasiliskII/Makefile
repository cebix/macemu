# Makefile for creating Basilisk II distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION := $(shell sed <BasiliskII.spec -n '/^Version: */s///p')
RELEASE := $(shell sed <BasiliskII.spec -n '/^Release: */s///p')
VERNAME := BasiliskII-$(VERSION)

SRCARCHIVE := $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
SRCRPM := $(VERNAME)-$(RELEASE).src.rpm
BINRPM := $(VERNAME)-$(RELEASE).i386.rpm
AMIGAARCHIVE := $(VERNAME)-$(RELEASE).amiga.lha
BEOSPPCARCHIVE := $(VERNAME)-$(RELEASE).beosppc.zip
BEOSX86ARCHIVE := $(VERNAME)-$(RELEASE).beosx86.zip

TMPDIR := $(shell date +/tmp/build%M%S)
RPMDIR := /usr/src/redhat
ISODATE := $(shell date "+%Y-%m-%d %H:%M")
DOCS := $(shell sed <BasiliskII.spec -n '/^\%doc */s///p')
SRCS := src

default:
	@echo "This top-level Makefile is for creating Basilisk II distributions."
	@echo "If you want to install Basilisk II V$(VERSION) on your system, please follow"
	@echo "the instructions in the file INSTALL."
	@echo "If you want to create a Basilisk II V$(VERSION) distribution, type \"make help\""
	@echo "to get a list of possible targets."

help:
	@echo "The following targets are available:"
	@echo "  tarball  source tarball ($(SRCARCHIVE))"
	@echo "  rpm      source and binary RPMs ($(SRCRPM) and $(BINRPM))"
	@echo "  amiga    AmigaOS binary archive ($(AMIGAARCHIVE))"
	@echo "  beosppc  BeOS/ppc binary archive ($(BEOSPPCARCHIVE))"
	@echo "  beosx86  BeOS/x86 binary archive ($(BEOSX86ARCHIVE))"

clean:
	-rm -f $(SRCARCHIVE)
	-rm -f $(SRCRPM) $(BINRPM)
	-rm -f $(AMIGAARCHIVE) $(BEOSPPCARCHIVE) $(BEOSX86ARCHIVE)

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRCS) $(DOCS)
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	cd $(TMPDIR); cvs export -D "$(ISODATE)" BasiliskII
	rm -rf $(TMPDIR)/BasiliskII/src/powerrom_cpu	#not yet ready for distribution
	mv $(TMPDIR)/BasiliskII $(TMPDIR)/$(VERNAME)
	cd $(TMPDIR); tar cfz $@ $(VERNAME)
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# RPMs (source and i386 binary)
#
rpm: $(SRCRPM) $(BINRPM)

$(RPMDIR)/SOURCES/$(SRCARCHIVE): $(SRCARCHIVE)
	cp $(SRCARCHIVE) $(RPMDIR)/SOURCES

$(RPMDIR)/SRPMS/$(SRCRPM) $(RPMDIR)/RPMS/i386/$(BINRPM): $(RPMDIR)/SOURCES/$(SRCARCHIVE) BasiliskII.spec
	rpm -ba BasiliskII.spec

$(SRCRPM): $(RPMDIR)/SRPMS/$(SRCRPM)
	cp $(RPMDIR)/SRPMS/$(SRCRPM) .

$(BINRPM): $(RPMDIR)/RPMS/i386/$(BINRPM)
	cp $(RPMDIR)/RPMS/i386/$(BINRPM) .

#
# Binary archive for AmigaOS
#
amiga: $(AMIGAARCHIVE)

$(AMIGAARCHIVE): $(SRCS) $(DOCS) src/AmigaOS/BasiliskII
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	mkdir $(TMPDIR)/$(VERNAME)
	cp $(DOCS) $(TMPDIR)/$(VERNAME)
	cp src/AmigaOS/BasiliskII $(TMPDIR)/$(VERNAME)
	cp src/AmigaOS/BasiliskII.info $(TMPDIR)/$(VERNAME)
	chmod 775 $(TMPDIR)/$(VERNAME)/BasiliskII
	cd $(TMPDIR); lha a $@ $(VERNAME)
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# Binary archive for BeOS/ppc
#
beosppc: $(BEOSPPCARCHIVE)

$(BEOSPPCARCHIVE): $(SRCS) $(DOCS) src/BeOS/obj.ppc/BasiliskII
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	mkdir $(TMPDIR)/$(VERNAME)
	cp $(DOCS) $(TMPDIR)/$(VERNAME)
	cp src/BeOS/obj.ppc/BasiliskII $(TMPDIR)/$(VERNAME)
	mimeset -f $(TMPDIR)
	cd $(TMPDIR); zip -ry $@ $(VERNAME)/
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# Binary archive for BeOS/x86
#
beosx86: $(BEOSX86ARCHIVE)

$(BEOSX86ARCHIVE): $(SRCS) $(DOCS) src/BeOS/obj.x86/BasiliskII
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	mkdir $(TMPDIR)/$(VERNAME)
	cp $(DOCS) $(TMPDIR)/$(VERNAME)
	cp src/BeOS/obj.x86/BasiliskII $(TMPDIR)/$(VERNAME)
	mimeset -f $(TMPDIR)
	cd $(TMPDIR); zip -ry $@ $(VERNAME)/
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)
