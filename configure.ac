AC_INIT([msitools], [0.01])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libmsi/sql-parser.y])
AM_INIT_AUTOMAKE([foreign -Wno-portability])

AC_CANONICAL_HOST
AS_CASE([$host], [*-*-mingw*|*-*-cygwin*], [win32=yes], [win32=no])
AM_CONDITIONAL([WIN32], [test $win32 = yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_YACC

AM_PATH_GLIB_2_0([2.12.0])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0])
PKG_CHECK_MODULES([GSF], [libgsf-1])
PKG_CHECK_MODULES([UUID], [uuid], [uuid=yes], [uuid=no])
AS_IF([test "$uuid" = yes],
      [AC_DEFINE([HAVE_LIBUUID], 1,
                 [Define to 1 if libuuid is present on the system])],
      [AC_MSG_WARN([libuuid not found, msibuild will generate packages without a GUID])])

LT_INIT([win32-dll disable-fast-install])
GOBJECT_INTROSPECTION_CHECK([0.9.4])
AM_CONDITIONAL([GIR], [test "x$INTROSPECTION_MAKEFILE" != x])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_MISSING_PROG([AUTOM4TE], [autom4te])
AC_CONFIG_TESTDIR([.])
AC_CONFIG_FILES([atlocal])
AC_CONFIG_FILES([tests/package.m4.tmp:tests/package.m4.in],
                [$srcdir/build-aux/move-if-change tests/package.m4.tmp $srcdir/tests/package.m4
                 cp $srcdir/tests/package.m4 tests/package.m4.tmp])

AC_CONFIG_FILES([Makefile libmsi/Makefile tests/Makefile])
AC_OUTPUT
