NULL =
bin_PROGRAMS = wixl

AM_CFLAGS = -w

# --vapidir paths are relative to the source directory!

AM_VALAFLAGS =					\
	-H wixl.h --use-header			\
	--vapidir=.				\
	--vapidir=../vapi			\
	--vapidir=$(abs_top_builddir)/libmsi	\
	--pkg config				\
	--enable-experimental			\
	--pkg gio-2.0				\
	--pkg libmsi-1.0			\
	--pkg libgcab-1.0			\
	--pkg libxml-2.0			\
	--pkg posix				\
	$(NULL)

wixl_SOURCES =					\
	builder.vala				\
	msi.vala				\
	preprocessor.vala			\
	util.vala				\
	wix.vala				\
	wixl.vala				\
	$(NULL)

AM_CPPFLAGS =					\
	-include config.h			\
	-I $(top_srcdir)/include		\
	$(WIXL_CFLAGS)				\
	-DG_LOG_DOMAIN=\""wixl"\"		\
	-DLOCALEDIR=\""$(localedir)"\"		\
	-DPKGDATADIR=\""$(pkgdatadir)"\"	\
	-DPKGLIBDIR=\""$(pkglibdir)"\"		\
	$(NULL)

wixl_LDADD =					\
	$(WIXL_LIBS)				\
	../libmsi/libmsi.la			\
	$(NULL)

wixl_DEPENDENCIES = ../libmsi/libmsi.la
