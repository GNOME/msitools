ACLOCAL_AMFLAGS = -I m4
SUBDIRS = libmsi . tests

noinst_HEADERS = include/debug.h
dist_include_HEADERS = include/libmsi.h

AM_CPPFLAGS = -Iinclude -I$(srcdir)/include $(GLIB_CFLAGS) $(GSF_CFLAGS) $(UUID_CFLAGS)

AM_LDFLAGS = -Llibmsi

bin_PROGRAMS = msibuild msiinfo

msibuild_SOURCES = tools/msibuild.c
msibuild_LDADD = -lmsi $(GLIB_LIBS) $(GSF_LIBS) $(UUID_LIBS)
msibuild_DEPENDENCIES = libmsi/libmsi.la

msiinfo_SOURCES = tools/msiinfo.c
msiinfo_LDADD = -lmsi $(GLIB_LIBS) $(GSF_LIBS)
msiinfo_DEPENDENCIES = libmsi/libmsi.la
