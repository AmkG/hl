#define DEFINE_GLOBALS
#include"all_defines.hpp"

#ifndef single_threaded
	bool single_threaded;
#endif

class Symbol;

Symbol* symbol_sym;
Symbol* symbol_int;
Symbol* symbol_char;
Symbol* symbol_cons;
Symbol* symbol_pid;
Symbol* symbol_fn;
Symbol* symbol_string;
Symbol* symbol_unspecified;
Symbol* symbol_float;
Symbol* symbol_array;
Symbol* symbol_table;
Symbol* symbol_container;
Symbol* symbol_bytecode;
Symbol* symbol_iport;
Symbol* symbol_oport;
Symbol* symbol_binobj;

#include"symbols.hpp"

boost::scoped_ptr<SymbolsTable> symbols;

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
	symbol_float = symbols->lookup("<hl>float");
	symbol_array = symbols->lookup("<hl>array");
	symbol_table = symbols->lookup("<hl>table");
	symbol_container = symbols->lookup("<hl>container");
        symbol_bytecode = symbols->lookup("<hl>bytecode");
        symbol_iport = symbols->lookup("<hl>iport");
        symbol_oport = symbols->lookup("<hl>oport");
        symbol_binobj = symbols->lookup("<hl>binobj");
}

