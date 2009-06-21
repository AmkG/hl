#ifndef HISTORY_H
#define HISTORY_H

#include"objects.hpp"

#define BOOST_CB_DISABLE_DEBUG

#include <boost/circular_buffer.hpp>

class Process;
class GenericTraverser;
#include <iostream>
/*
 * Keep the history of the last function calls
 */
class History {
private:
	typedef boost::circular_buffer<Object::ref> inner_ring;
	typedef boost::circular_buffer<inner_ring> outer_ring;
	outer_ring ring;
	size_t breadth;

public:
	History(size_t depth, size_t breadth);

	// reset the history
	void reset();

	// record a function (closure) call -- may delete the oldest call
	void enter(Object::ref clos);

	// record a function call in tail position
	void enter_tail(Object::ref clos);

	// record a function return (i.e. a continuation call)
	void leave();

	// push a list of the last functions called in the process stack
	void to_list(Process & proc);

	// traverse the history
	void traverse(GenericTraverser* gt);
};

#endif // HISTORY_H
