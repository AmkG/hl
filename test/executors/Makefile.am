CPPFLAGS = -I@BOOST_PATH@ -I../../inc -I../../os
LDFLAGS = @PTHREAD_LIB@

bin_PROGRAMS = run_bytecode
run_bytecode_SOURCES = \
	../../src/globals.cpp \
	../../src/symbols.cpp \
	../../src/heaps.cpp \
	../../src/symtable.cpp \
	../../src/processes.cpp \
	../../src/types.cpp \
	../../src/executors.cpp \
	../../src/reader.cpp \
	../../src/workers.cpp \
	../../inc/all_defines.hpp \
	../../inc/bytecodes.hpp \
	../../inc/executors.hpp \
	../../inc/generics.hpp \
	../../inc/hashes.hpp \
	../../inc/heaps.hpp \
	../../inc/intrusives.hpp \
	../../inc/lockeds.hpp \
	../../inc/mutexes.hpp \
	../../inc/objects.hpp \
	../../inc/processes.hpp \
	../../inc/reader.hpp \
	../../inc/specializeds.hpp \
	../../inc/symbols.hpp \
	../../inc/types.hpp \
	../../inc/unichars.hpp \
	../../inc/workarounds.hpp \
	../../inc/workers.hpp \
	../../os/thread.hpp \
	run_bytecode.cpp
