/*
This header should be the first header in each source file,
and should only be included once
*/

#define __STDC_LIMIT_MACROS // necessary for INTPTR_MAX and INTPTR_MIN

#include<boost/scoped_ptr.hpp>

#ifndef DEFINE_GLOBALS

class SymbolsTable;

extern boost::scoped_ptr<SymbolsTable> symbols;

#ifndef single_threaded
	extern bool single_threaded;
#endif

#endif


