
AC_DEFUN([CHECK_GEM5],
    [AC_ARG_WITH(gem5, AC_HELP_STRING([--with-gem5], [compile with gem5 support - libgem5_opt.so required]))

    CPPFLAGS="-std=c++11"
    
    AC_SUBST([GEM5_CFLAGS], [])
    AC_SUBST([GEM5_LDFLAGS], [])



    check_gem5=yes
    
    if test x"${with_gem5}" == xno; then
        check_gem5=no
    elif test x"${with_gem5}" == x; then
        check_gem5=no
    elif test x"${with_gem5}" != x; then
        CFLAGS="${CFLAGS} -I${with_gem5}"
        CPPFLAGS="${CPPFLAGS} -I${with_gem5}"

        LDFLAGS="${LDFLAGS} -L${with_gem5}/ -lgem5_opt -Wl,-rpath,${with_gem5}"

        gem5_path=${with_gem5}
    fi

    if test x"${check_gem5}" == xyes; then
        AC_LANG_PUSH([C++])
        AC_CHECK_HEADER([mem/cache/dummy_cache.hh], [], [AC_MSG_ERROR([gem5 dummy_cache.hh not found! Did you run ./install_gem5mod.sh first?])])
        AC_CHECK_LIB(gem5_opt, main, [], [AC_MSG_ERROR([gem5 libgem5_opt.so not found! Did you recompile gem5?])])
        AC_CHECK_FILE([${gem5_path}/sim/cxx_config.cc], [], [AC_MSG_ERROR([gem5 cxx_config.cc not found! Did you recompile with --with-cxx-config?])])

        AC_CHECK_FILE

        LDFLAGS="${LDFLAGS} ${gem5_path}/sim/cxx_config.os"

        #CPPFLAGS="${CPPFLAGS} ${gem5_path}/sim/cxx_config.cc"
        AC_LANG_POP

        AC_DEFINE(HAVE_GEM5, 1, enables gem5)
        AC_MSG_NOTICE([gem5 support enabled])
        #AC_SUBST([GEM5_CFLAGS], [])
        #AC_SUBST([GEM5_LDFLAGS], [-lgem5_opt])
        #if test x${gem5_path} != x; then
        #    AC_SUBST([GEM5_CFLAGS], [-I${gem5_path}/ sim/cxx_config.cc])
        #    AC_SUBST([GEM5_LDFLAGS], ["-L${gem5_path} -lgem5_opt"])
        #fi


    fi


    ]
)
