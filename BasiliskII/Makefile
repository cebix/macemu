# Makefile for creating Basilisk II distributions
# Written in 1999 by Christian Bauer <Christian.Bauer@uni-mainz.de>

SRCARCHIVE = $(shell date +BasiliskII_src_%d%m%Y.tar.gz)
BUILDDIR = $(shell echo /tmp/build$$)

#
# Source tarball
#
srcdist: $(SRCARCHIVE)

$(SRCARCHIVE): src CHANGES COPYING README TECH TODO
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
# RPM source archive
#
srcrpm: $(SRCARCHIVE)

#
# RPM binary archive (Unix/i386)
#
i386rpm:
