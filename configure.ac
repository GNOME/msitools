AC_INIT([wixl],[0.0], [marcandre.lureau@gmail.com])
AC_CONFIG_SRCDIR([src/wixl.vala])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign -Wall -Werror subdir-objects -Wno-portability])

# Enable silent rules is available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CC_STDC
AM_PROG_VALAC([0.16])
AC_PROG_INSTALL
IT_PROG_INTLTOOL([0.35])
PKG_PROG_PKG_CONFIG

pkg_modules="
        gio-2.0 >= 2.26.0
        libmsi-1.0
        libgcab-1.0
        uuid >= 1.41.3
        libxml-2.0 >= 2.9
"
PKG_CHECK_MODULES(WIXL, [$pkg_modules])
AC_SUBST(WIXL_CFLAGS)
AC_SUBST(WIXL_LIBS)

GETTEXT_PACKAGE=wixl
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT_VERSION([1.11])
AM_GLIB_GNU_GETTEXT

AM_MISSING_PROG([AUTOM4TE], [autom4te])
AC_CONFIG_TESTDIR([.])
AC_CONFIG_FILES([atlocal])
AC_CONFIG_FILES([tests/package.m4.tmp:tests/package.m4.in],
                [$srcdir/build-aux/move-if-change tests/package.m4.tmp $srcdir/tests/package.m4
                 cp $srcdir/tests/package.m4 tests/package.m4.tmp])

CFLAGS="$CFLAGS -w"

AC_CONFIG_FILES([
	Makefile
	po/Makefile.in
	src/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([
        wixl $VERSION
        ============

        prefix:                   ${prefix}
        c compiler:               ${CC}
])
