#ifndef HISTORY_H
#define HISTORY_H

#include"objects.hpp"

#define BOOST_CB_DISABLE_DEBUG

#include <boost/circular_buffer.hpp>

/*
 * Keep the history of the last function calls
 */
class History {
private:
	typedef boost::circular_buffer<Object::ref> inner_ring;
	typedef boost::circular_buffer<inner_ring> outer_ring;
	outer_ring ring;

public:
	History(size_t depth, size_t breadth) : ring(sz) {}

	// record a function (closure) call -- may delete the oldest call
	inline void enter(Object::ref clos) {
		ring.push_back(outer_ring(breadth, 1, clos));
	}

	// record a function call in tail position
	inline void enter_tail(Object::ref clos) {
		inner_ring::iterator last = ring.end()-1;
		last->push_back(clos);
	}

	// record a function return (i.e. a continuation call)
	inline void leave() {
		ring.pop_back();
	}

	// push a list of the last functions called in the process stack
	void to_list(Process & proc);

	// traverse the history
	void traverse(GenericTraverser* gt);
};

void History::to_list(Process & proc) {
	size_t count = 0;
	for (outer_ring::reverse_iterator i = ring.rbegin(); i != ring.rend(); ++i) {
		for (inner_ring::reverse_iterator j = i->rbegin(); j != i->rend(); ++i) {
			proc.stack.push(*j);
			count++;
		}
	}
	proc.stack.push(Object::nil());
	// build the list
	while (count-- > 0) {
		bytecode_cons(proc, proc.stack);
	}
}

void History::traverse(GenericTraverser *gt) {
	for (outer_ring::reverse_iterator i = ring.rbegin(); i != ring.rend(); ++i) {
		for (inner_ring::reverse_iterator j = i->rbegin(); j != i->rend(); ++i) {
			gt->traverse(*j);
		}
	}
}

#endif // HISTORY_H
