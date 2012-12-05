AC_INIT([msitools], [0.0])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libmsi/sql-parser.y])
AM_INIT_AUTOMAKE([foreign])

AC_CANONICAL_HOST
AS_CASE([$host], [*-*-mingw*|*-*-cygwin*], [win32=yes], [win32=no])
AM_CONDITIONAL([WIN32], [test $win32 = yes])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_YACC

LT_INIT([win32-dll disable-fast-install])

AC_CONFIG_FILES([Makefile libmsi/Makefile tests/Makefile])
AC_CONFIG_FILES([tests/runtest], [chmod +x tests/runtest])
AC_OUTPUT
