# Makefile for creating Basilisk II distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

SRCARCHIVE = $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
AMIGAARCHIVE = $(shell date +BasiliskII_amiga_%d%m%Y.lha)
BEOSPPCARCHIVE = $(shell date +BasiliskII_beos_ppc_%d%m%Y.zip)
BEOSX86ARCHIVE = $(shell date +BasiliskII_beos_x86_%d%m%Y.zip)

BUILDDIR = /tmp/build
DOCS = ChangeLog COPYING INSTALL README TECH TODO
SRCS = src

default:
	@echo "This top-level Makefile is for creating Basilisk II distributions."
	@echo "If you want to install Basilisk II on your system, please follow"
	@echo "the instructions in the file INSTALL."
	@echo "If you want to create a Basilisk II distribution, type \"make help\""
	@echo "to get a list of possible targets."

help:
	@echo "The following targets are available:"
	@echo "  tarball  source tarball ($(SRCARCHIVE))"
	@echo "  rpm      source and binary RPMs"
	@echo "  amiga    AmigaOS binary archive ($(AMIGAARCHIVE))"
	@echo "  beosppc  BeOS/ppc binary archive ($(BEOSPPCARCHIVE))"
	@echo "  beosx86  BeOS/x86 binary archive ($(BEOSX86ARCHIVE))"

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRCS) $(DOCS)
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	cd $(BUILDDIR); cvs checkout BasiliskII
	rm -rf $(BUILDDIR)/BasiliskII/src/powerrom_cpu	#not yet ready for distribution
	cd $(BUILDDIR); tar cfz $@ BasiliskII
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# RPMs
#
rpm: /usr/src/redhat/SOURCES/$(SRCARCHIVE) BasiliskII.spec
	rpm -ba BasiliskII.spec

/usr/src/redhat/SOURCES/$(SRCARCHIVE): $(SRCARCHIVE)
	cp $(SRCARCHIVE) /usr/src/redhat/SOURCES

#
# Binary archive for AmigaOS
#
amiga: $(AMIGAARCHIVE)

$(AMIGAARCHIVE): $(SRCS) $(DOCS) src/AmigaOS/BasiliskII
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	mkdir $(BUILDDIR)/BasiliskII
	cp $(DOCS) $(BUILDDIR)/BasiliskII
	cp src/AmigaOS/BasiliskII $(BUILDDIR)/BasiliskII
	cp src/AmigaOS/BasiliskII.info $(BUILDDIR)/BasiliskII.info
	cd $(BUILDDIR); lha a $@ BasiliskII
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# Binary archive for BeOS/ppc
#
beosppc: $(BEOSPPCARCHIVE)

$(BEOSPPCARCHIVE): $(SRCS) $(DOCS) src/BeOS/obj.ppc/BasiliskII
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	mkdir $(BUILDDIR)/BasiliskII
	cp $(DOCS) $(BUILDDIR)/BasiliskII
	mv src/BeOS/obj.ppc/BasiliskII $(BUILDDIR)/BasiliskII
	cd $(BUILDDIR); zip -ry $@ BasiliskII/
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# Binary archive for BeOS/x86
#
beosx86: $(BEOSX86ARCHIVE)

$(BEOSX86ARCHIVE): $(SRCS) $(DOCS) src/BeOS/obj.x86/BasiliskII
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	mkdir $(BUILDDIR)/BasiliskII
	cp $(DOCS) $(BUILDDIR)/BasiliskII
	mv src/BeOS/obj.x86/BasiliskII $(BUILDDIR)/BasiliskII
	cd $(BUILDDIR); zip -ry $@ BasiliskII/
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)
