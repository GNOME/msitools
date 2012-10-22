lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(srcdir) -I. -DUNICODE
AM_CFLAGS = -Wunused -Wimplicit
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

noinst_HEADERS = \
	debug.h \
	list.h \
	msipriv.h \
	msiquery.h \
	msiserver.h \
	query.h \
	unicode.h

libmsi_la_SOURCES = alter.c create.c database.c delete.c distinct.c \
	drop.c handle.c insert.c msiquery.c record.c select.c storages.c \
	streams.c string.c suminfo.c table.c tokenize.c update.c where.c \
	sql-parser.y \
	msiserver_i.c \
	utf16/snprintfW.c utf16/sprintfW.c utf16/strtolW.c

libmsi_la_LDFLAGS = -no-undefined
libmsi_la_LIBADD = \
	$(libmsi_la_DEPENDENCIES) \
	-lshlwapi \
	-lole32
