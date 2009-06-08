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
	boost::circular_buffer<Object::ref> ring;
	typedef boost::circular_buffer<Object::ref>::reverse_iterator ring_it;
public:
	History(size_t sz) : ring(sz) {}

	// record a function (closure) call -- may delete the oldest call
	inline void enter(Object::ref clos) {
		ring.push_back(clos);
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
	for (ring_it i = ring.rbegin(); i != ring.rend(); ++i) {
		proc.stack.push(*i);
		count++;
	}
	proc.stack.push(Object::nil());
	// build the list
	while (count-- > 0) {
		bytecode_cons(proc, proc.stack);
	}
}

void History::traverse(GenericTraverser *gt) {
	for (ring_it i = ring.rbegin(); i != ring.rend(); ++i) {
		gt->traverse(*i);
	}
}

#endif // HISTORY_H
