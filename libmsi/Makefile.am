lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(srcdir) -I. -D__WINESRC__ -DUNICODE -D_UNICODE
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

noinst_HEADERS = \
	msi.h \
	msipriv.h \
	msiquery.h \
	msiserver.h \
	query.h

libmsi_la_SOURCES = alter.c create.c database.c delete.c distinct.c \
	drop.c handle.c insert.c media.c msi.c msi_main.c msiquery.c \
	package.c record.c select.c source.c \
	storages.c streams.c string.c suminfo.c table.c tokenize.c update.c \
	where.c \
	sql-parser.y \
	msiserver_i.c

libmsi_la_DEPENDENCIES = \
	msi.res

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

%.res: %.rc
	$(RC) $(RCFLAGS) -o $@ $<
