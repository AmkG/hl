#ifndef SYMBOLS_H
#define SYMBOLS_H

#include"objects.hpp"
#include"heaps.hpp"
#include"mutexes.hpp"

#include<vector>
#include<string>
#include<map>

class SymbolsTable;

class Symbol {
	ValueHolderRef value;
	std::string printname; //utf-8
	AppMutex m;

	Symbol(); //disallowed
	explicit Symbol(std::string x) : printname(x) {};

	std::vector<Process*> notification_list;

public:
	void copy_value_to(ValueHolderRef&);
	void copy_value_to_and_add_notify(ValueHolderRef&, Process*);
	void set_value(Object::ref);
	std::string getPrintName() { return printname; } // for debugging
	/*WARNING! not thread safe. intended for use during soft-stop*/
	void traverse_objects(HeapTraverser* ht) {
		value->traverse_objects(ht);
	}
	friend class SymbolsTable;
};

class SymbolsTableTraverser {
public:
	virtual void traverse(Symbol*) =0;
	virtual ~SymbolsTableTraverser() { }
};

class SymbolsTable {
private:
	std::map<std::string, Symbol*> tb;
	AppMutex m;

public:
	Symbol* lookup(std::string);
	inline Symbol* lookup(char const* s) {
		return lookup(std::string(s));
	}
	/*note! no atomicity checks.  assumes that traversal
	occurs only when all other worker threads are suspended.
	*/
	void traverse_symbols(SymbolsTableTraverser*) const;
	~SymbolsTable();
};

#endif //SYMBOLS_H

