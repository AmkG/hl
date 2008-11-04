#ifndef SYMBOLS_H
#define SYMBOLS_H

#include"objects.hpp"

#include<string>
#include<map>

#include<boost/scoped_ptr.hpp>

class ValueHolder;

class SymbolsTable;

class Symbol {
	boost::scoped_ptr<ValueHolder> value;
	std::string printname; //utf-8
	/*
	insert locking object here
	*/

	Symbol(); //disallowed
	explicit Symbol(std::string x) : printname(x) {};
public:
	void copy_value_to(boost::scoped_ptr<ValueHolder>&);
	void set_value(Object::ref);
	friend class SymbolsTable;
};

class SymbolsTable {
private:
	std::map<std::string, Symbol*> tb;
	/*
	insert locking object here
	*/
public:
	Symbol* lookup(std::string);
	inline Symbol* lookup(char const* s) {
		return lookup(std::string(s));
	}
	~SymbolsTable();
};

#endif //SYMBOLS_H

