NULL =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = --no-print-directory

SUBDIRS =					\
	src					\
	po					\
	$(NULL)

EXTRA_DIST =					\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	$(NULL)

DISTCLEANFILES =				\
	intltool-extract			\
	intltool-merge				\
	intltool-update				\
	$(NULL)

# Autotest support

TESTSUITE = $(srcdir)/tests/testsuite
EXTRA_DIST += tests/testsuite.at $(srcdir)/tests/package.m4 $(TESTSUITE)
DISTCLEANFILES += atconfig
CLEANFILES += testsuite.log

check-local: $(TESTSUITE) atconfig
	builddir=`pwd` && \
		$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$$builddir/src $(TESTSUITEFLAGS)

installcheck-local: $(TESTSUITE) atconfig
	$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$(bindir) $(TESTSUITEFLAGS)

clean-local:
	-$(SHELL) $(TESTSUITE) --clean

$(TESTSUITE): tests/testsuite.at tests/package.m4
	$(AUTOM4TE) --language=autotest -I$(builddir)/tests -o $@.tmp $@.at
	mv $@.tmp $@
