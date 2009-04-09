OTHER_INCLUDES =
if BOOST_PATH_SPECIFIED
OTHER_INCLUDES += -I@BOOST_PATH@
endif

AM_CPPFLAGS = $(OTHER_INCLUDES) -I../inc -I../os -I..
LDFLAGS = @PTHREAD_LIB@

noinst_LIBRARIES = libhlvm.a
libhlvm_a_SOURCES = \
	globals.cpp \
	symbols.cpp \
	heaps.cpp \
	symtable.cpp \
	main.cpp \
	processes.cpp \
	generics.cpp \
	types.cpp \
	executors.cpp \
	reader.cpp \
	workers.cpp \
	../inc/all_defines.hpp \
	../inc/bytecodes.hpp \
	../inc/executors.hpp \
	../inc/generics.hpp \
	../inc/hashes.hpp \
	../inc/heaps.hpp \
	../inc/intrusives.hpp \
	../inc/lockeds.hpp \
	../inc/mutexes.hpp \
	../inc/objects.hpp \
	../inc/processes.hpp \
	../inc/reader.hpp \
	../inc/specializeds.hpp \
	../inc/symbols.hpp \
	../inc/types.hpp \
	../inc/unichars.hpp \
	../inc/workarounds.hpp \
	../inc/workers.hpp \
	../os/thread.hpp

bin_PROGRAMS = hl
hl_SOURCES = main.cpp
hl_LDADD = libhlvm.a
