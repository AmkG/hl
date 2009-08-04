#ifndef HISTORY_H
#define HISTORY_H

#include"objects.hpp"

#define BOOST_CB_DISABLE_DEBUG

#include <boost/circular_buffer.hpp>
#include <vector>

class Process;
class ProcessStack;
class GenericTraverser;

/*
 * Keep the history of the last function calls
 */
class History {
private:
	class Item {
	public:
		Object::ref clos; // registered function
		std::vector<Object::ref> args; // args passed to function
	};

	typedef boost::circular_buffer<Item> inner_ring;
	typedef boost::circular_buffer<inner_ring> outer_ring;
	outer_ring ring;
	size_t breadth;

	// record an argument for the current function call
	void push_arg(Object::ref arg);

public:
	History(size_t depth, size_t breadth);

	// reset the history
	void reset();

	// record a function (closure) call -- may delete the oldest call
	void enter(Object::ref clos);

	// record a function call in tail position
	void enter_tail(Object::ref clos);

	// record function call arguments held in stack
	void register_args(ProcessStack & stack, int from, int to);

	// record a function return (i.e. a continuation call)
	void leave();

	// push a list of the last functions called in the process stack
	void to_list(Process & proc);

	// traverse the history
	void traverse(GenericTraverser* gt);
};

#endif // HISTORY_H
