NULL =
lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir) -I. \
	      $(GLIB_CFLAGS) $(GSF_CFLAGS) $(GOBJECT_CFLAGS)

AM_CFLAGS = -Wunused -Wimplicit
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

noinst_HEADERS =				\
	list.h					\
	msipriv.h				\
	query.h

libmsi_la_SOURCES =				\
	alter.c					\
	create.c				\
	delete.c				\
	distinct.c				\
	drop.c					\
	insert.c				\
	libmsi-database.c			\
	libmsi-query.c				\
	libmsi-record.c				\
	libmsi-summary-info.c			\
	select.c				\
	sql-parser.y				\
	storages.c				\
	streams.c				\
	string.c				\
	table.c					\
	tokenize.c				\
	update.c				\
	where.c					\
	$(NULL)

libmsi_la_LDFLAGS = -no-undefined -rpath $(libdir) \
		    -export-symbols-regex='^libmsi_'

libmsi_la_LIBADD = $(GLIB_LIBS) $(GSF_LIBS) $(GOBJECT_LIBS)
