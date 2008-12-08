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
	symbol_cons = symbols->lookup("<hl>cons");
	symbol_pid = symbols->lookup("<hl>pid");
	symbol_fn = symbols->lookup("<hl>fn");
	symbol_string = symbols->lookup("<hl>string");
	symbol_unspecified = symbols->lookup("<hl>unpsecified");
	symbol_num = symbols->lookup("<hl>num");
	symbol_array = symbols->lookup("<hl>array");
	symbol_table = symbols->lookup("<hl>table");
	symbol_container = symbols->lookup("<hl>container");
}

