AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(srcdir) -I. \
	      $(GLIB_CFLAGS) $(GOBJECT_CFLAGS)

dist_noinst_HEADERS = test.h
noinst_PROGRAMS = testrecord testdatabase

testrecord_SOURCES = testrecord.c
testrecord_LDADD = ../libmsi/libmsi.la
testdatabase_SOURCES = testdatabase.c
testdatabase_LDADD = ../libmsi/libmsi.la

if WIN32
noinst_PROGRAMS += testsuminfo

testdatabase_LDADD += -lole32
testsuminfo_SOURCES = testsuminfo.c
testsuminfo_LDADD = ../libmsi/libmsi.la -lole32
endif
