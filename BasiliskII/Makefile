# Makefile for creating Basilisk II distributions
# Written in 2002 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION := $(shell sed <BasiliskII.spec -n '/^\%define version */s///p')
RELEASE := $(shell sed <BasiliskII.spec -n '/^\%define release */s///p')
VERNAME := BasiliskII-$(VERSION)

SRCARCHIVE := $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
AMIGAARCHIVE := $(VERNAME)-$(RELEASE).amiga.lzh
BEOSPPCARCHIVE := $(VERNAME)-$(RELEASE).beosppc.zip
BEOSX86ARCHIVE := $(VERNAME)-$(RELEASE).beosx86.zip
MACOSXARCHIVE := $(VERNAME)-$(RELEASE).tar.gz

TMPDIR := $(shell date +/tmp/build%M%S)
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
	@echo "  rpm      source and binary RPMs"
	@echo "  amiga    AmigaOS binary archive ($(AMIGAARCHIVE))"
	@echo "  beosppc  BeOS/ppc binary archive ($(BEOSPPCARCHIVE))"
	@echo "  beosx86  BeOS/x86 binary archive ($(BEOSX86ARCHIVE))"
	@echo "  macosx   MacOS X binary archive ($(MACOSXARCHIVE))"

clean:
	-rm -f $(SRCARCHIVE)
	-rm -f $(AMIGAARCHIVE) $(BEOSPPCARCHIVE) $(BEOSX86ARCHIVE)

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRCS) $(DOCS)
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	cd $(TMPDIR); cvs export -D "$(ISODATE)" BasiliskII
	cd $(TMPDIR)/BasiliskII/src/Unix && NO_CONFIGURE=1 ./autogen.sh
	cd $(TMPDIR)/BasiliskII/src/Windows && NO_CONFIGURE=1 ../Unix/autogen.sh
	rm $(TMPDIR)/BasiliskII/Makefile
	mv $(TMPDIR)/BasiliskII $(TMPDIR)/$(VERNAME)
	cd $(TMPDIR); tar cfz $@ $(VERNAME)
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)

#
# RPMs (source and binary)
#
rpm: $(SRCARCHIVE)
	rpmbuild -ta --clean $(SRCARCHIVE)

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

#
# Binary archive for MacOS X
#
macosx: $(MACOSXARCHIVE)

$(MACOSXARCHIVE): $(SRCS) $(DOCS) src/MacOSX/build/BasiliskII.app
	-rm -rf $(TMPDIR)
	mkdir $(TMPDIR)
	mkdir $(TMPDIR)/$(VERNAME)
	cp $(DOCS) $(TMPDIR)/$(VERNAME)
	cp -pr src/MacOSX/build/BasiliskII.app $(TMPDIR)/$(VERNAME)
	cd $(TMPDIR); tar -czvf $@ $(VERNAME)/
	mv $(TMPDIR)/$@ .
	rm -rf $(TMPDIR)
