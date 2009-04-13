/*
This header should be the first header in each source file,
and should only be included once
*/

#define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN

#include<boost/scoped_ptr.hpp>
#include<boost/shared_ptr.hpp>

#ifndef DEFINE_GLOBALS

class SymbolsTable;
class Symbol;
class IOPort;

extern boost::scoped_ptr<SymbolsTable> symbols;
extern Symbol* symbol_sym;
extern Symbol* symbol_int;
extern Symbol* symbol_char;
extern Symbol* symbol_cons;
extern Symbol* symbol_pid;
extern Symbol* symbol_fn;
extern Symbol* symbol_string;
extern Symbol* symbol_unspecified;
extern Symbol* symbol_float;
extern Symbol* symbol_array;
extern Symbol* symbol_table;
extern Symbol* symbol_container;
extern Symbol* symbol_bytecode;
extern Symbol* symbol_ioport;
extern Symbol* symbol_event;
extern Symbol* symbol_binobj;
extern Symbol* symbol_io;
extern Symbol* symbol_call_star;

extern boost::shared_ptr<IOPort> port_stdin;
extern boost::shared_ptr<IOPort> port_stdout;
extern boost::shared_ptr<IOPort> port_stderr;

#ifndef single_threaded
	extern bool single_threaded;
#endif

void initialize_globals(void);

#endif


