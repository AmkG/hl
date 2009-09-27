OTHER_INCLUDES =
if BOOST_PATH_SPECIFIED
OTHER_INCLUDES += -I@BOOST_PATH@
endif

AM_CPPFLAGS = $(OTHER_INCLUDES) -I@srcdir@/../inc -I@srcdir@/../os -I@srcdir@/..\
	-DLIBEXECDIR=\"@libexecdir@\" -DEXE_EXT=\"$(EXEEXT)\" -DDATADIR=\"@datadir@\"
LDFLAGS = @PTHREAD_LIB@

noinst_LIBRARIES = libhlvm.a
libhlvm_a_SOURCES = \
	aio.cpp \
	globals.cpp \
	ios_posix.cpp \
	symbols.cpp \
	heaps.cpp \
	symtable.cpp \
	main.cpp \
	processes.cpp \
	types.cpp \
	executors.cpp \
	reader.cpp \
	workers.cpp \
	assembler.cpp \
	history.cpp \
	unichars.cpp \
	../inc/aio.hpp \
	../inc/all_defines.hpp \
	../inc/assembler.hpp \
	../inc/bytecodes.hpp \
	../inc/executors.hpp \
	../inc/generics.hpp \
	../inc/hashes.hpp \
	../inc/heaps.hpp \
	../inc/history.hpp \
	../inc/intrusives.hpp \
	../inc/lockeds.hpp \
	../inc/mutexes.hpp \
	../inc/obj_aio.hpp \
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
hl_SOURCES = hl.cpp

libexec_PROGRAMS = hlvma
hlvma_SOURCES = main.cpp
hlvma_LDADD = libhlvm.a

