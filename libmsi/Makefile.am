NULL =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmsi-1.0.pc
lib_LTLIBRARIES = libmsi.la

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include -I$(srcdir) -I. \
	      $(GLIB_CFLAGS) $(GSF_CFLAGS) $(GOBJECT_CFLAGS)

AM_CFLAGS = $(WARN_CFLAGS)
AM_YFLAGS = -d
BUILT_SOURCES = sql-parser.c sql-parser.h

libmsi_la_SOURCES =				\
	alter.c					\
	create.c				\
	debug.c					\
	debug.h					\
	delete.c				\
	distinct.c				\
	drop.c					\
	insert.c				\
	libmsi-database.c			\
	libmsi-enums.c				\
	libmsi-istream.h			\
	libmsi-istream.c			\
	libmsi-query.c				\
	libmsi-record.c				\
	libmsi-summary-info.c			\
	list.h					\
	msipriv.h				\
	query.h					\
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
		    -export-symbols-regex '^libmsi_'

libmsi_la_LIBADD = $(GLIB_LIBS) $(GSF_LIBS) $(GOBJECT_LIBS)

EXTRA_DIST = libmsi-enums.c.etemplate

INTROSPECTION_SCANNER_ARGS = --warn-all

if GIR
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = Libmsi-1.0.gir

Libmsi-1.0.gir: libmsi.la
Libmsi_1_0_gir_NAMESPACE = Libmsi
Libmsi_1_0_gir_VERSION = 1.0
Libmsi_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
Libmsi_1_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0
Libmsi_1_0_gir_LIBS = libmsi.la
Libmsi_1_0_gir_FILES =					\
	$(top_srcdir)/include/libmsi-database.h		\
	$(top_srcdir)/include/libmsi-query.h		\
	$(top_srcdir)/include/libmsi-record.h		\
	$(top_srcdir)/include/libmsi-summary-info.h	\
	$(top_srcdir)/include/libmsi-types.h		\
	$(top_srcdir)/include/libmsi.h			\
	$(top_srcdir)/libmsi/libmsi-database.c		\
	$(top_srcdir)/libmsi/libmsi-query.c		\
	$(top_srcdir)/libmsi/libmsi-record.c		\
	$(top_srcdir)/libmsi/libmsi-summary-info.c	\
	$(top_builddir)/include/libmsi-enums.h		\
	libmsi-enums.c					\
	$(NULL)
Libmsi_1_0_gir_EXPORT_PACKAGES = libmsi-1.0
Libmsi_1_0_gir_SCANNERFLAGS = --c-include="libmsi.h"

girdir = $(datadir)/gir-1.0
gir_DATA = Libmsi-1.0.gir
typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)

if VAPI
vapidir = $(datadir)/vala/vapi
vapi_DATA = libmsi-1.0.vapi
BUILT_SOURCES += $(vapi_DATA)
EXTRA_DIST += Libmsi-1.0.metadata

libmsi-1.0.vapi: Libmsi-1.0.gir Libmsi-1.0.metadata
	$(AM_V_GEN)$(VAPIGEN) -q		\
		--metadatadir=$(srcdir)		\
		--vapidir=$(builddir)		\
		--library libmsi-1.0		\
		--pkg gio-2.0			\
		$<

CLEANFILES += libmsi-1.0.vapi
endif # VAPI

endif # GIR
