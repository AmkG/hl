# init
AC_INIT([hl], [0.0], [])
AM_INIT_AUTOMAKE([-Wall foreign])

# check for C++ compiler
AC_PROG_CXX
AC_PROG_CXXCPP
AC_LANG([C++])

# basic conf
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])



# user options
AC_ARG_WITH([boost-include], [AS_HELP_STRING([--with-boost-include], [path where boost headers are installed])], [CPPFLAGS=-I${withval}], [])

# check for pthreads
AC_CHECK_HEADERS([pthread.h])

# check for BOOST libraries
AC_CHECK_HEADERS([boost/scoped_ptr.hpp boost/shared_ptr.hpp boost/smart_ptr.hpp boost/scoped_ptr.hpp boost/noncopyable.hpp boost/intrusive_ptr.hpp], [], [AC_MSG_ERROR([You need to install BOOST (http://www.boost.org)])])

AC_OUTPUT
