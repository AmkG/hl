OTHER_INCLUDES =
if BOOST_PATH_SPECIFIED
OTHER_INCLUDES += -I@BOOST_PATH@
endif

AM_CPPFLAGS = $(OTHER_INCLUDES) -I@srcdir@/../../inc -I@srcdir@/../../os
LDFLAGS = @PTHREAD_LIB@

bin_PROGRAMS = driver
driver_SOURCES = driver.cpp
driver_LDADD = ../../src/libhlvm.a
