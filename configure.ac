AC_PREREQ([2.65])
AC_INIT([lgs],[2.0.0],[digirols@inf.ethz.ch])

AM_INIT_AUTOMAKE([1.10])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/simulator/main.cpp])
AC_CONFIG_HEADERS([src/config.h])
#AC_DEFINE(HAVE_GEM5, 0, disable gem5 for now)


AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CXX_C_O


AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_LIB(ps, PS_open_file, , AC_MSG_ERROR(libps not found!))
AC_CHECK_LIB(cgraph, agnxtnode, , AC_MSG_ERROR(libcgraph not found!))


AM_CXXFLAGS=-O3 -g -pedantic -Wno-deprecated -Wall -Wno-long-long -std=c++11

AX_BOOST_BASE
AX_BOOST_REGEX

CHECK_GEM5

if test "x$link_regex" != "xyes"; then
    AC_MSG_ERROR("Drawviz cannot be compiled without boost:regex!")
fi

AC_CONFIG_FILES([Makefile src/DrawViz/Makefile src/p4schedgen/Makefile src/schedgen/Makefile src/simulator/Makefile src/txt2bin/Makefile])



AC_OUTPUT
