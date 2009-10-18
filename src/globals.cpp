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
Symbol* symbol_string_builder;
Symbol* symbol_string_pointer;
Symbol* symbol_unspecified;
Symbol* symbol_float;
Symbol* symbol_array;
Symbol* symbol_table;
Symbol* symbol_container;
Symbol* symbol_bytecode;
Symbol* symbol_ioport;
Symbol* symbol_event;
Symbol* symbol_binobj;
Symbol* symbol_io;
Symbol* symbol_bool;
Symbol* symbol_call_star;

#include"symbols.hpp"

boost::scoped_ptr<SymbolsTable> symbols;

#include"aio.hpp"

boost::shared_ptr<IOPort> port_stdin;
boost::shared_ptr<IOPort> port_stdout;
boost::shared_ptr<IOPort> port_stderr;

void initialize_globals(void) {
	symbols.reset(new SymbolsTable());
	symbol_sym = symbols->lookup("<hl>sym");
	symbol_int = symbols->lookup("<hl>int");
	symbol_char = symbols->lookup("<hl>char");
	symbol_cons = symbols->lookup("<hl>cons");
	symbol_pid = symbols->lookup("<hl>pid");
	symbol_fn = symbols->lookup("<hl>fn");
	symbol_string = symbols->lookup("<hl>string");
	symbol_string_builder = symbols->lookup("<hl>string-builder");
	symbol_string_pointer = symbols->lookup("<hl>string-pointer");
	symbol_unspecified = symbols->lookup("<hl>unspecified");
	symbol_float = symbols->lookup("<hl>float");
	symbol_array = symbols->lookup("<hl>array");
	symbol_table = symbols->lookup("<hl>table");
	symbol_container = symbols->lookup("<hl>container");
        symbol_bytecode = symbols->lookup("<hl>bytecode");
        symbol_ioport = symbols->lookup("<hl>ioport");
        symbol_event = symbols->lookup("<hl>event");
	symbol_binobj = symbols->lookup("<hl>binobj");
	symbol_io = symbols->lookup("<hl>i/o");
        symbol_bool = symbols->lookup("<hl>bool");
        symbol_call_star = symbols->lookup("<hl>call*");

	aio_initialize();

	port_stdin = ioport_stdin();
	port_stdout = ioport_stdout();
	port_stderr = ioport_stderr();
}

