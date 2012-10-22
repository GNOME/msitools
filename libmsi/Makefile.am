lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(srcdir) -I. -DUNICODE
AM_CFLAGS = -Wunused -Wimplicit
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

noinst_HEADERS = \
	msi.h \
	msipriv.h \
	msiquery.h \
	msiserver.h \
	query.h

libmsi_la_SOURCES = alter.c create.c database.c delete.c distinct.c \
	drop.c handle.c insert.c msi.c msiquery.c record.c select.c \
	storages.c streams.c string.c suminfo.c table.c tokenize.c update.c \
	where.c \
	sql-parser.y \
	msiserver_i.c \
	utf16/snprintfW.c utf16/sprintfW.c utf16/strtolW.c

libmsi_la_LDFLAGS = -no-undefined
libmsi_la_LIBADD = \
	$(libmsi_la_DEPENDENCIES) \
	-luuid \
	-lurlmon \
	-lwininet \
	-lcomctl32 \
	-lshell32 \
	-lshlwapi \
	-lcabinet \
	-loleaut32 \
	-lole32 \
	-lversion \
	-luser32 \
	-lgdi32 \
	-ladvapi32 \
	-lodbccp32 \
	-lwintrust \
	-lcrypt32 \
	-limagehlp
