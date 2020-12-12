# Makefile for creating Basilisk II distributions
# Written in 2002 by Christian Bauer <Christian.Bauer@uni-mainz.de>

VERSION := $(shell sed <BasiliskII.spec -n '/^\%define version */s///p')
RELEASE := $(shell sed <BasiliskII.spec -n '/^\%define release */s///p')
VERNAME := BasiliskII-$(VERSION)

SRCARCHIVE := $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
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
	@echo "  macosx   MacOS X binary archive ($(MACOSXARCHIVE))"

clean:
	-rm -f $(SRCARCHIVE)

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
