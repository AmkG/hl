/*
This header should be the first header in each source file,
and should only be included once
*/

#define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN

#include<boost/scoped_ptr.hpp>

/*
One source file, probably the main source file, should
define "DEFINE_GLOBALS" in order to actually instantiate
the global variables
*/
#ifndef DEFINE_GLOBALS
#define EXTERN extern
#else
#define EXTERN
#endif

class SymbolsTable;

EXTERN boost::scoped_ptr<SymbolsTable> symbols;

#ifndef single_threaded
	EXTERN bool single_threaded;
#endif

