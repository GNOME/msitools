lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(srcdir) -I. -D__WINESRC__ -DUNICODE -D_UNICODE
AM_YFLAGS = -d
BUILT_SOURCES = msiserver.h msiserver_i.c sql-parser.c sql-parser.h cond-parser.c cond-parser.h

noinst_HEADERS = \
	msi.h \
	msipriv.h \
	msiquery.h \
	msiserver_dispids.h \
	msiserver.h \
	query.h

libmsi_la_SOURCES = action.c alter.c appsearch.c assembly.c \
	automation.c classes.c create.c custom.c database.c delete.c \
	dialog.c distinct.c drop.c events.c files.c font.c format.c \
	handle.c insert.c install.c media.c msi.c msi_main.c msiquery.c \
	package.c patch.c record.c registry.c script.c select.c source.c \
	storages.c streams.c string.c suminfo.c table.c tokenize.c update.c \
	upgrade.c where.c \
	cond-parser.y sql-parser.y \
	msiserver_i.c

libmsi_la_DEPENDENCIES = \
	msiserver_r.res \
	msiserver_t.res \
	msi.res

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

msi.res: msi.rc msiserver.rgs instabsent.bmp instadvert.bmp instlocal.bmp

%.res: %.rc
	$(RC) $(RCFLAGS) -o $@ $<
%.h: %.idl
	$(WIDL) $(IDLFLAGS) -h -o $@ $<
%_c.c: %.idl
	$(WIDL) $(IDLFLAGS) -c -o $@ $<
%_i.c: %.idl
	$(WIDL) $(IDLFLAGS) -u -o $@ $<
%_p.c: %.idl
	$(WIDL) $(IDLFLAGS) -p -o $@ $<
%_r.res: %.idl
	$(WIDL) $(IDLFLAGS) -r -o $@ $<
%_s.c: %.idl
	$(WIDL) $(IDLFLAGS) -s -o $@ $<
%.tlb: %.idl
	$(WIDL) $(TARGETFLAGS) $(IDLFLAGS) -t -o $@ $<
%_t.res: %.idl
	$(WIDL) $(TARGETFLAGS) $(IDLFLAGS) -t -o $@ $<
