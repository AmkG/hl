/*
This header should be the first header in each source file,
and should only be included once
*/

#include"config.h"

#ifdef HAVE_PERMISSIVE_CLIMITS
# define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN
# include<stdint.h>
# include<climits>
#endif

#ifdef HAVE_INT_POINTERS
  typedef int intptr_t;
# include<climits>
# define INTPTR_MIN INT_MIN
# define INTPTR_MAX INT_MAX
#endif

#ifdef HAVE_LONG_POINTERS
  typedef long intptr_t;
# include<climits>
# define INTPTR_MIN LONG_MIN
# define INTPTR_MAX LONG_MAX
#endif

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
extern Symbol* symbol_string_builder;
extern Symbol* symbol_string_pointer;
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
extern Symbol* symbol_bool;
extern Symbol* symbol_call_star;

extern boost::shared_ptr<IOPort> port_stdin;
extern boost::shared_ptr<IOPort> port_stdout;
extern boost::shared_ptr<IOPort> port_stderr;

#ifndef single_threaded
	extern bool single_threaded;
#endif

void initialize_globals(void);

#endif


