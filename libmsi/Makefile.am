lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir) -I. \
	      $(GLIB_CFLAGS) $(GSF_CFLAGS)

AM_CFLAGS = -Wunused -Wimplicit
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

noinst_HEADERS = \
	list.h \
	msipriv.h \
	query.h

libmsi_la_SOURCES = alter.c create.c database.c delete.c distinct.c \
	drop.c handle.c insert.c msiquery.c record.c select.c storages.c \
	streams.c string.c suminfo.c table.c tokenize.c update.c where.c \
	sql-parser.y

libmsi_la_LDFLAGS = -no-undefined -rpath $(libdir) \
		    -export-symbols-regex='^libmsi_'

libmsi_la_LIBADD = $(GLIB_LIBS) $(GSF_LIBS)
