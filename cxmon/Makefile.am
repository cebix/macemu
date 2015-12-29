## Process this file with automake to produce Makefile.in
SUBDIRS = src

# Requires automake 1.5
AUTOMAKE_OPTIONS = foreign 1.5

# Manpages
man_MANS = cxmon.1

EXTRA_DIST = cxmon.1

dist-hook: cxmon.spec
	cp cxmon.spec $(distdir)

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: dist

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpm -ta --clean $(PACKAGE)-$(VERSION).tar.gz
