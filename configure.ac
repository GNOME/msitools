AC_INIT([msitools], [0.90])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libmsi/sql-parser.y])
AM_INIT_AUTOMAKE([foreign -Wno-portability dist-xz])

AC_CANONICAL_HOST
AS_CASE([$host], [*-*-mingw*|*-*-cygwin*], [win32=yes], [win32=no])
AM_CONDITIONAL([WIN32], [test $win32 = yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_YACC

IT_PROG_INTLTOOL([0.35])

AM_PATH_GLIB_2_0([2.12.0])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 gio-2.0 >= 2.14])
PKG_CHECK_MODULES([GSF], [libgsf-1])
PKG_CHECK_MODULES([UUID], [uuid >= 1.41.3])

LT_INIT([win32-dll disable-fast-install])
GOBJECT_INTROSPECTION_CHECK([0.9.4])
AM_CONDITIONAL([GIR], [test "x$INTROSPECTION_MAKEFILE" != x])

AM_PROG_VALAC([0.16])
AC_PATH_PROG(VAPIGEN, vapigen, no)
AC_SUBST(VAPIGEN)
AM_CONDITIONAL([VAPI], [test "x$VAPIGEN" != xno])

AS_IF([test $win32 = no],
      [WIXL_UNIX="gio-unix-2.0"])

PKG_CHECK_MODULES([WIXL], [gio-2.0 >= 0.23.0
                           libgcab-1.0
                           uuid >= 1.41.3
                           libxml-2.0 >= 2.7
                           $WIXL_UNIX])

GETTEXT_PACKAGE=wixl
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT_VERSION([0.18])
AM_GLIB_GNU_GETTEXT

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_MISSING_PROG([AUTOM4TE], [autom4te])
AC_CONFIG_TESTDIR([.])
AC_CONFIG_FILES([atlocal])
AC_CONFIG_FILES([tests/package.m4.tmp:tests/package.m4.in],
                [$srcdir/build-aux/move-if-change tests/package.m4.tmp $srcdir/tests/package.m4
                 cp $srcdir/tests/package.m4 tests/package.m4.tmp])

AC_CONFIG_FILES([
    Makefile
    include/Makefile
    data/Makefile
    po/Makefile.in
    libmsi/Makefile
    libmsi/libmsi-1.0.pc
    tests/Makefile
])
AC_OUTPUT

AC_MSG_NOTICE([
        msitools $VERSION
        ================

        prefix:                   ${prefix}
        c compiler:               ${CC}
])
