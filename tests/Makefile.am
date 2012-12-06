AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir) -I.

noinst_PROGRAMS = testrecord testdatabase

testrecord_SOURCES = testrecord.c
testrecord_LDADD = ../libmsi/libmsi.la
testdatabase_SOURCES = testdatabase.c
testdatabase_LDADD = ../libmsi/libmsi.la

TESTS_ENVIRONMENT = srcdir=$(srcdir) ./runtest

if WIN32
noinst_PROGRAMS += testsuminfo

testdatabase_LDADD += -lole32
testsuminfo_SOURCES = testsuminfo.c
testsuminfo_LDADD = ../libmsi/libmsi.la -lole32

TESTS = \
	testdatabase$(EXEEXT) \
	testrecord$(EXEEXT) \
	testsuminfo$(EXEEXT)

endif

CLEANFILES = \
	testdatabase.log testdatabase.diff \
	testrecord.log testrecord.diff \
	testsuminfo.log testsuminfo.diff

EXTRA_DIST = runtest \
	testdatabase.ok \
	testrecord.ok \
	testsuminfo.ok

.NOTPARALLEL:
