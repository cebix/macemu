# Makefile for creating Basilisk II distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

SRCARCHIVE = $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
AMIGAARCHIVE = $(shell date +BasiliskII_amiga_%d%m%Y.lha)
BEOSPPCARCHIVE = $(shell date +BasiliskII_beos_ppc_%d%m%Y.zip)
BEOSX86ARCHIVE = $(shell date +BasiliskII_beos_x86_%d%m%Y.zip)

BUILDDIR = /tmp/build
DOCS = CHANGES COPYING README TECH TODO
SRC = src

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRC) $(DOCS)
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	cd $(BUILDDIR); cvs checkout BasiliskII
	rm $(BUILDDIR)/BasiliskII/BasiliskII.spec
	rm $(BUILDDIR)/BasiliskII/Makefile
	rm -rf $(BUILDDIR)/BasiliskII/src/powerrom_cpu
	cd $(BUILDDIR); tar cfz $@ BasiliskII
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# Source RPM
#
srcrpm: $(SRCARCHIVE) BasiliskII.spec

#
# Binary RPM for Unix/i386
#
i386rpm:

#
# Binary archive for AmigaOS
#
amiga: $(AMIGAARCHIVE)

$(AMIGAARCHIVE): $(SRC) $(DOCS) src/AmigaOS/BasiliskII
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	mkdir $(BUILDDIR)/BasiliskII
	cp $(DOCS) $(BUILDDIR)/BasiliskII
	cp src/AmigaOS/BasiliskII $(BUILDDIR)/BasiliskII
	cp src/AmigaOS/BasiliskII.info $(BUILDDIR)/BasiliskII.info
	cd $(BUILDDIR); lha av $@ BasiliskII
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# Binary archive for BeOS/ppc
#
beosppc: $(BEOSPPCARCHIVE)

$(BEOSPPCARCHIVE): $(SRC) $(DOCS) src/BeOS/obj.ppc/BasiliskII
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

$(BEOSX86ARCHIVE): $(SRC) $(DOCS) src/BeOS/obj.x86/BasiliskII
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	mkdir $(BUILDDIR)/BasiliskII
	cp $(DOCS) $(BUILDDIR)/BasiliskII
	mv src/BeOS/obj.x86/BasiliskII $(BUILDDIR)/BasiliskII
	cd $(BUILDDIR); zip -ry $@ BasiliskII/
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)
