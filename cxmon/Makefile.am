## Process this file with automake to produce Makefile.in
SUBDIRS = src

# Manpages
man_MANS = cxmon.1

EXTRA_DIST = cxmon.1 bootstrap

dist-hook: cxmon.spec
	cp cxmon.spec $(distdir)

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz
