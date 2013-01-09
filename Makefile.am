ACLOCAL_AMFLAGS = -I m4
SUBDIRS = libmsi tests .

if WIXL
SUBDIRS += po wixl
endif

dist_include_HEADERS = \
	include/libmsi.h \
	include/libmsi-database.h \
	include/libmsi-query.h \
	include/libmsi-record.h \
	include/libmsi-summary-info.h \
	include/libmsi-types.h

AM_CPPFLAGS = -Iinclude -I$(srcdir)/include $(GLIB_CFLAGS) $(GSF_CFLAGS) $(UUID_CFLAGS)

AM_LDFLAGS = -Llibmsi

bin_PROGRAMS = msibuild msiinfo

msibuild_SOURCES = tools/msibuild.c tools/sqldelim.c tools/sqldelim.h
msibuild_LDADD = -lmsi $(GLIB_LIBS) $(GSF_LIBS) $(UUID_LIBS)
msibuild_DEPENDENCIES = libmsi/libmsi.la

msiinfo_SOURCES = tools/msiinfo.c
msiinfo_LDADD = -lmsi $(GLIB_LIBS) $(GSF_LIBS)
msiinfo_DEPENDENCIES = libmsi/libmsi.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmsi-1.0.pc

# Autotest support

dist_noinst_DATA = tests/testsuite.at tests/package.m4 tests/testsuite
DISTCLEANFILES = atconfig atlocal
CLEANFILES = testsuite.log

check-local: $(srcdir)/tests/testsuite atconfig atlocal
	$(SHELL) $(srcdir)/tests/testsuite AUTOTEST_PATH=.:wixl $(TESTSUITEFLAGS)

installcheck-local: $(srcdir)/tests/testsuite atconfig atlocal
	$(SHELL) $(srcdir)/tests/testsuite AUTOTEST_PATH=$(bindir) $(TESTSUITEFLAGS)

clean-local:
	-$(SHELL) $(srcdir)/tests/testsuite --clean

$(srcdir)/tests/testsuite: tests/testsuite.at tests/package.m4
	cd $(srcdir)/tests && \
		$(AUTOM4TE) --language=autotest -o testsuite.tmp testsuite.at && \
		mv testsuite.tmp testsuite
