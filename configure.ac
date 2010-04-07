# init
AC_INIT([hl], [0.0], [])
AM_INIT_AUTOMAKE([-Wall foreign])

# check for C++ compiler
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_RANLIB
AC_LANG([C++])

# basic conf
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([inc/config.h])
AC_CONFIG_FILES([Makefile src/Makefile test/Makefile test/executors/Makefile test/disassembler/Makefile test/reader/Makefile hl2b/Makefile])

# user options
AC_ARG_WITH([boost-include], [AS_HELP_STRING([--with-boost-include], [path where boost headers are installed])],
  [boost_path=$withval; boost_path_specified=true; CPPFLAGS="$CPPFLAGS -I${withval}"],
  [boost_path_specified=false])
AC_SUBST(BOOST_PATH, $boost_path)
AM_CONDITIONAL(BOOST_PATH_SPECIFIED, test x$boost_path_specified = xtrue)

# check for random libraries
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h unistd.h errno.h signal.h stddef.h],[],[AC_MSG_ERROR([Your system is not as POSIX-compliant as we expected])])

# check for pthreads
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_LIB([pthread], [pthread_create], [AC_SUBST([PTHREAD_LIB], ["-lpthread"])], [AC_DEFINE([single_threaded], [1], [run in a single OS thread])])

# check for BOOST libraries
AC_CHECK_HEADERS([boost/scoped_ptr.hpp boost/shared_ptr.hpp boost/smart_ptr.hpp boost/scoped_ptr.hpp boost/noncopyable.hpp boost/intrusive_ptr.hpp boost/circular_buffer.hpp],
  [],
  [AC_MSG_ERROR([You need to install BOOST (http://www.boost.org) at least version 1.35.00])])

# checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([bool],
  [],
  [AC_MSG_ERROR([Your C++ compiler inexplicably does not support "bool"])])
AC_CHECK_TYPES([ptrdiff_t],
  [],
  [AC_MSG_ERROR([Your libraries inexplicably do not have "ptrdiff_t"])],
[
#include<stddef.h>
])

AC_MSG_CHECKING([how to get an intptr_t type with ranges])
intptr_type=no
if test $intptr_type = no; then
AC_COMPILE_IFELSE(
[[
#define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN
#include<stdint.h>
#include<climits>
intptr_t tmp;
#ifndef INTPTR_MIN
# error grr
#endif
#ifndef INTPTR_MAX
# error grr
#endif
int main(void) { return 0; }
]],
  [intptr_type="stdint"
  AC_DEFINE([HAVE_PERMISSIVE_CLIMITS], [],
    [Define this if stdint.h provides intptr_t and defining __STDC_LIMIT_MACROS makes stdint.h give correct ranges to intptr_t in C++]
  )
])
fi
if test $intptr_type = no; then
AC_COMPILE_IFELSE(
[[
#include<climits>
#ifndef INT_MIN
# error grr
#endif
#ifndef INT_MAX
# error grr
#endif
int main(void) {
  int test_array[
    ((long int) sizeof(int)) == ((long int) sizeof(void*)) ? 1 : -1
  ];
  return 0;
}
]],
  [intptr_type="int"
  AC_DEFINE([HAVE_INT_POINTERS], [],
    [Define this if type "int" are the same size as pointers]
  )
])
fi
if test $intptr_type = no; then
AC_COMPILE_IFELSE(
[[
#include<climits>
#ifndef LONG_MIN
# error grr
#endif
#ifndef LONG_MAX
# error grr
#endif
int main(void) {
  int test_array[
    ((long int) sizeof(long)) == ((long int) sizeof(void*)) ? 1 : -1
  ];
  return 0;
}
]],
  [intptr_type="long"
  AC_DEFINE([HAVE_LONG_POINTERS], [],
    [Define this if type "long" are the same size as pointers]
  )
])
fi
if test $intptr_type = no; then
  AC_MSG_ERROR([We can't find out what size pointers are])
fi
AC_MSG_RESULT([$intptr_type])

# checks for library functions.
AC_CHECK_FUNCS([select strchr],[],[AC_MSG_ERROR([Your system libraries do not provide some necessary functions])])

# some definitions that just have to be
AC_DEFINE([single_threaded], [], [Define this for now because almkglor is lousy at multithreaded])

AC_OUTPUT
