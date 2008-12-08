/*
This header should be the first header in each source file,
and should only be included once
*/

#define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN

#include<boost/scoped_ptr.hpp>

#ifndef DEFINE_GLOBALS

class SymbolsTable;
class Symbol;

extern boost::scoped_ptr<SymbolsTable> symbols;
extern Symbol* symbol_sym;
extern Symbol* symbol_int;
extern Symbol* symbol_char;
extern Symbol* symbol_cons;
extern Symbol* symbol_pid;
extern Symbol* symbol_fn;
extern Symbol* symbol_string;
extern Symbol* symbol_unspecified;
extern Symbol* symbol_num;
extern Symbol* symbol_array;
extern Symbol* symbol_table;
extern Symbol* symbol_container;

#ifndef single_threaded
	extern bool single_threaded;
#endif

void initialize_globals(void);

#endif


