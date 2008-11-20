#define DEFINE_GLOBALS
#include"all_defines.hpp"

#ifndef single_threaded
	bool single_threaded;
#endif

#include"symbols.hpp"

boost::scoped_ptr<SymbolsTable> symbols;

