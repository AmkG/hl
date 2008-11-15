#include"all_defines.hpp"

#include"symbols.hpp"
#include"mutexes.hpp"

#include<string>
#include<map>

typedef std::map<std::string, Symbol*> maptype;

Symbol* SymbolsTable::lookup(std::string x) {
	{AppLock l(m);
		Symbol*& s = tb[x];
		if(s) return s;
		s = new Symbol(x);
		return s;
	}
}

SymbolsTable::~SymbolsTable() {
	/*go through the table and delete each symbol*/
	for(maptype::const_iterator it = tb.begin(); it != tb.end(); ++it) {
		delete it->second;
	}
}

