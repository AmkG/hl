#ifndef SYMBOLS_H
#define SYMBOLS_H

#include"objects.hpp"

#include<string>

#include<boost/scoped_ptr.hpp>

class ValueHolder;

class Symbol {
	boost::scoped_ptr<ValueHolder> value;
	std::string printname; //utf-8
	/*
	insert locking object here
	*/

	Symbol() { }
	explicit Symbol(string);
public:
	static Symbol* lookup(std::string);
	static inline Symbol* lookup(char const* s) {
		return lookup(std::string(s));
	}
	void copy_value_to(boost::scoped_ptr<ValueHolder>&);
	void set_value(Object::ref);
};

#endif //SYMBOLS_H

