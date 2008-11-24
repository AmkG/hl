#include"all_defines.hpp"

#include"symbols.hpp"
#include"mutexes.hpp"

#include<boost/noncopyable.hpp>

#include<string>
#include<map>

void Symbol::copy_value_to(ValueHolderRef& p) {
	{AppLock l(m);
		/*Unfortunately the entire cloning has to be
		done while we have the lock.  This is because
		a Symbol::set_value() might invalidate the
		pointer from under us if we didn't do the
		entire cloning locked.
		Other alternatives exist: we can use a locked
		reference counting scheme, or use some sort
		of deferred deallocation.
		*/
		value->clone(p);
	}
}

void Symbol::set_value(Object::ref o) {
	ValueHolderRef tmp;
	ValueHolder::copy_object(tmp, o);
	{AppLock l(m);
		value.swap(tmp);
		/*TODO: perform notification here*/
	}
}

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

