
#include"symbols.hpp"

#include<string>
#include<map>

Symbol* SymbolsTable::lookup(std::string x) {
	Symbol*& s = tb[x];
	if(s) return s;
	s = new Symbol(x);
	return s;
}

SymbolsTable::~SymbolsTable() {
	/*go through the*/
}

