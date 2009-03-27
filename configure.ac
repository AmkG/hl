AC_INIT([hl], [0.0], [])
AM_INIT_AUTOMAKE([-Wall foreign])
AC_PROG_CXX
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_AUXDIR([build])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CHECK_HEADERS([
        boost/scoped_ptr.hpp 
        boost/shared_ptr.hpp 
        boost/smart_ptr.hpp 
        boost/scoped_ptr.hpp 
        boost/noncopyable.hpp 
        boost/intrusive_ptr.hpp])
AC_SUBST([BOOST_HEADERS])
AC_OUTPUT
