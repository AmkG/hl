#include"all_defines.hpp"

#include"symbols.hpp"
#include"mutexes.hpp"

#include<boost/noncopyable.hpp>

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

void SymbolsTable::traverse_symbols(SymbolsTableTraverser* stt) const {
	for(maptype::const_iterator it = tb.begin(); it != tb.end(); ++it) {
		stt->traverse(it->second);
	}
}

class SymbolDeletor : public SymbolsTableTraverser {
public:
	virtual void traverse(Symbol* s) {
		delete s;
	}
};

SymbolsTable::~SymbolsTable() {
	/*go through the table and delete each symbol*/
	SymbolDeletor sd;
	traverse_symbols(&sd);
}

