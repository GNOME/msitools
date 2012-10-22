AC_INIT([msitools], [0.0])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libmsi/sql.y])
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_YACC

AC_CHECK_TOOLS([WIDL], [widl],
               [AC_MSG_ERROR([could not find widl])])

AC_ARG_VAR([RCFLAGS], [Flags for the Windows resource compiler])
AC_ARG_VAR([IDLFLAGS], [Flags for the Windows IDL compiler])

LT_INIT([win32-dll disable-fast-install])
LT_LANG([Windows Resource])

AC_CONFIG_FILES([Makefile libmsi/Makefile])
AC_OUTPUT
