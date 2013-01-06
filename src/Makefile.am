NULL =
bin_PROGRAMS = wixl

AM_CFLAGS = -w

AM_VALAFLAGS =					\
	--vapidir=$(srcdir) --pkg config	\
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
	util.vala				\
	wix.vala				\
	wixl.vala				\
	$(NULL)

AM_CPPFLAGS =					\
	-include config.h			\
	$(WIXL_CFLAGS)				\
	-DG_LOG_DOMAIN=\""wixl"\"		\
	-DLOCALEDIR=\""$(localedir)"\"		\
	-DPKGDATADIR=\""$(pkgdatadir)"\"	\
	-DPKGLIBDIR=\""$(pkglibdir)"\"		\
	$(NULL)

wixl_LDADD =					\
	$(WIXL_LIBS)				\
	$(NULL)
