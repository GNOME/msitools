AC_INIT([msitools],
        m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [https://bugzilla.gnome.org/enter_bug.cgi?product=msitools])
AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libmsi/sql-parser.y])
AM_INIT_AUTOMAKE([foreign -Wno-portability dist-xz])
AM_MAINTAINER_MODE([enable])

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

PKG_CHECK_MODULES([WIXL], [gio-2.0 >= 0.23.0
                           libgcab-1.0 >= 0.1.10
                           uuid >= 1.41.3
                           libxml-2.0 >= 2.7])

GETTEXT_PACKAGE=msitools
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext Package])
AC_SUBST(GETTEXT_PACKAGE)
AM_GNU_GETTEXT_VERSION([0.18])
AM_GLIB_GNU_GETTEXT

gl_MANYWARN_ALL_GCC([warnings])
nw=
nw="$nw -Wsystem-headers"       # Don't let system headers trigger warnings
nw="$nw -Wundef"                # All compiler preprocessors support #if UNDEF
nw="$nw -Wtraditional"          # All compilers nowadays support ANSI C
nw="$nw -Wstrict-overflow"      # yacc/bison generated code uses that..
nw="$nw -Wconversion"           # These warnings usually don't point to mistakes.
nw="$nw -Wsign-conversion"      # Likewise.
nw="$nw -Wunused-parameter"     # Likewise.
nw="$nw -Wunused-local-typedefs" # Likewise.
nw="$nw -Wunused-macros"        # Likewise.
nw="$nw -Wunsafe-loop-optimizations" # Likewise.
# Enable all GCC warnings not in this list.
gl_MANYWARN_COMPLEMENT([warnings], [$warnings], [$nw])
for w in $warnings; do
  gl_WARN_ADD([$w])
done
gl_WARN_ADD([-Wno-sign-compare])
gl_WARN_ADD([-Wno-unused-parameter])
gl_WARN_ADD([-Wno-missing-field-initializers])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_MISSING_PROG([AUTOM4TE], [autom4te])
AC_CONFIG_TESTDIR([.])
AC_CONFIG_FILES([atlocal])
AC_CONFIG_FILES([tests/package.m4.tmp:tests/package.m4.in],
                [$srcdir/build-aux/move-if-change tests/package.m4.tmp $srcdir/tests/package.m4
                 cp $srcdir/tests/package.m4 tests/package.m4.tmp])

AC_CONFIG_FILES([tools/msidump], [chmod +x tools/msidump])
AC_CONFIG_FILES([tools/msidiff], [chmod +x tools/msidiff])
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
