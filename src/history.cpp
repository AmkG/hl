#include "all_defines.hpp"
#include "history.hpp"
#include "processes.hpp"
#include "bytecodes.hpp"
#include "reader.hpp"

History::History(size_t depth, size_t breadth) : 
	ring(depth), breadth(breadth) {
	ring.push_back(inner_ring());
	ring.rbegin()->set_capacity(breadth);
}

void History::reset() {
	ring.erase(ring.begin(), ring.end());
}

void History::enter(Object::ref clos) {
	inner_ring i(breadth);
	i.push_back(clos);
	ring.push_back(i);
	// ?? for some reason, after insertion capacity must be set again
	ring.rbegin()->set_capacity(breadth);
}

void History::enter_tail(Object::ref clos) {
 	ring.rbegin()->push_back(clos);
}

void History::leave() {
	if (!ring.empty()) {
		ring.pop_back();
	}
}

void History::to_list(Process & proc) {
	size_t count = 0; // number of elements in the history
	int sz = ring.size();
	for (outer_ring::iterator i = ring.begin()+sz-1; sz>0; --i, --sz) {
		int sz = i->size();
		for (inner_ring::iterator j = i->begin()+sz-1; sz>0; --j, --sz) {
			proc.stack.push(*j);
			count++;
		}
	}
	proc.stack.push(Object::nil());
	// build the list
	for (int i=0; i<count; ++i) {
		bytecode_cons(proc, proc.stack);
	}
}

void History::traverse(GenericTraverser *gt) {
	int sz = ring.size();
	for (outer_ring::iterator i = ring.begin()+sz-1; sz>0; --i, --sz) {
		int sz = i->size();
		for (inner_ring::iterator j = i->begin()+sz-1; sz>0; --j, --sz) {
			gt->traverse(*j);
		}
	}
}
