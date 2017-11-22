AC_PREREQ([2.65])
AC_INIT([lgs],[2.0.0],[digirols@inf.ethz.ch])

AM_INIT_AUTOMAKE([1.10])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([simulator/main.cpp])
AC_CONFIG_HEADERS([config.h])
#AC_DEFINE(HAVE_GEM5, 0, disable gem5 for now)


AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CXX_C_O


AC_FUNC_MALLOC
AC_FUNC_REALLOC

AC_CHECK_LIB(ps, PS_open_file, , AC_MSG_ERROR(libps not found!))
AC_CHECK_LIB(cgraph, agnxtnode, , AC_MSG_ERROR(libcgraph not found!))

#AC_CHECK_LIB(boost_regex, regex_match, , AC_MSG_ERROR(boost_regex not found!))

AX_BOOST_BASE
AX_BOOST_REGEX

AM_CXXFLAGS=-O3 -g -pedantic -Wno-deprecated -Wall -Wno-long-long -std=c++11

AC_CONFIG_FILES([Makefile DrawViz/Makefile p4schedgen/Makefile simulator/Makefile txt2bin/Makefile])



AC_OUTPUT
