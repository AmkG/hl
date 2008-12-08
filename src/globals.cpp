#define DEFINE_GLOBALS
#include"all_defines.hpp"

#ifndef single_threaded
	bool single_threaded;
#endif

#include"symbols.hpp"

boost::scoped_ptr<SymbolsTable> symbols;
Symbol* symbol_sym;
Symbol* symbol_int;
Symbol* symbol_char;

void initialize_globals(void) {
	symbols.reset(new SymbolsTable());
	symbol_sym = symbols->lookup("<hl>sym");
	symbol_sym = symbols->lookup("<hl>int");
	symbol_sym = symbols->lookup("<hl>char");
}

