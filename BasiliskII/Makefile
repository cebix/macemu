# Makefile for creating Basilisk II distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION = $(shell sed <BasiliskII.spec -n '/^Version: */s///p')
RELEASE = $(shell sed <BasiliskII.spec -n '/^Release: */s///p')

SRCARCHIVE = $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
SRCRPM = BasiliskII-$(VERSION)-$(RELEASE).src.rpm
I386RPM = BasiliskII-$(VERSION)-$(RELEASE).i386.rpm
AMIGAARCHIVE = BasiliskII-$(VERSION)-$(RELEASE).amiga.lha
BEOSPPCARCHIVE = BasiliskII-$(VERSION)-$(RELEASE).beosppc.zip
BEOSX86ARCHIVE = BasiliskII-$(VERSION)-$(RELEASE).beosx86.zip

BUILDDIR = /tmp/build
RPMDIR = /usr/src/redhat
DOCS = $(shell sed <BasiliskII.spec -n '/^\%doc */s///p')
SRCS = src

default:
	@echo "This top-level Makefile is for creating Basilisk II distributions."
	@echo "If you want to install Basilisk II V$(VERSION) on your system, please follow"
	@echo "the instructions in the file INSTALL."
	@echo "If you want to create a Basilisk II V$(VERSION) distribution, type \"make help\""
	@echo "to get a list of possible targets."

help:
	@echo "The following targets are available:"
	@echo "  tarball  source tarball ($(SRCARCHIVE))"
	@echo "  rpm      source and binary RPMs ($(SRCRPM) and $(I386RPM))"
	@echo "  amiga    AmigaOS binary archive ($(AMIGAARCHIVE))"
	@echo "  beosppc  BeOS/ppc binary archive ($(BEOSPPCARCHIVE))"
	@echo "  beosx86  BeOS/x86 binary archive ($(BEOSX86ARCHIVE))"

clean:
	-rm -f $(SRCARCHIVE)
	-rm -f $(SRCRPM)
	-rm -f $(AMIGAARCHIVE) $(BEOSPPCARCHIVE) $(BEOSX86ARCHIVE)

#
# Source tarball
#
tarball: $(SRCARCHIVE)

$(SRCARCHIVE): $(SRCS) $(DOCS)
	-rm -rf $(BUILDDIR)
	mkdir $(BUILDDIR)
	cd $(BUILDDIR); cvs checkout BasiliskII
	rm -rf $(BUILDDIR)/BasiliskII/src/powerrom_cpu	#not yet ready for distribution
	mv $(BUILDDIR)/BasiliskII $(BUILDDIR)/BasiliskII-$(VERSION)
	cd $(BUILDDIR); tar cfz $@ BasiliskII-$(VERSION)
	mv $(BUILDDIR)/$@ .
	rm -rf $(BUILDDIR)

#
# RPMs (source and i386 binary)
#
rpm: $(SRCRPM) $(I386RPM)

$(RPMDIR)/SOURCES/$(SRCARCHIVE): $(SRCARCHIVE)
	cp $(SRCARCHIVE) $(RPMDIR)/SOURCES

$(RPMDIR)/SRPMS/$(SRCRPM) $(RPMDIR)/RPMS/i386/$(I386RPM): $(RPMDIR)/SOURCES/$(SRCARCHIVE) BasiliskII.spec
	rpm -ba BasiliskII.spec

$(SRCRPM): $(RPMDIR)/SRPMS/$(SRCRPM)
	cp $(RPMDIR)/SRPMS/$(SRCRPM) .

$(I386RPM): $(RPMDIR)/RPMS/i386/$(I386RPM)
	cp $(RPMDIR)/RPMS/i386/$(I386RPM) .

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
