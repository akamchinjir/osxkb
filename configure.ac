AC_INIT([osxkb], [1.0], [akam.chinjir@gmail.com])

m4_bregexp(AC_PACKAGE_VERSION, [\([0-9]+\)\.\([0-9]+\)], [
  AC_DEFINE([MAJOR_VERSION], [\1], [define to the major version])
  AC_DEFINE([MINOR_VERSION], [\2], [define to the minor version])
])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror nostdinc])

AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL

AC_CONFIG_MACRO_DIR([m4])

WARN_CFLAGS="-Wall -Werror -Wtype-limits -Wuninitialized -Wconversion"
AC_SUBST([WARN_CFLAGS])

AC_HEADER_STDBOOL
if test $ac_cv_header_stdbool_h = no; then
  AC_MSG_ERROR([This package depends on a C99-conformant stdbool.h])
fi

AC_CHECK_FUNCS([stpcpy])

PKG_CHECK_MODULES([gio], [gio-2.0])

AC_CANONICAL_HOST
case "${host_os}" in
  cygwin*|mingw*)
    build_windows=yes
    ;;
esac

AM_CONDITIONAL([WINDOWS], [test "${build_windows}" = "yes"])

AC_CONFIG_FILES(Makefile
                data/Makefile
                tools/Makefile
                src/Makefile)

AC_OUTPUT
