#include "all_defines.hpp"
#include "history.hpp"
#include "processes.hpp"
#include "bytecodes.hpp"
#include "reader.hpp"

History::History(size_t depth, size_t breadth) : 
	ring(depth), breadth(breadth) {
	ring.push_back(inner_ring(breadth));
}

void History::reset() {
	ring.erase(ring.begin(), ring.end());
}

void History::enter(Object::ref clos) {
	inner_ring i(breadth);
	Item it;
	it.clos = clos;
	i.push_back(it);
	ring.push_back(i);
	// ?? for some reason, after insertion capacity must be set again
	ring.rbegin()->set_capacity(breadth);
}

void History::enter_tail(Object::ref clos) {
	Item i;
	i.clos = clos;
 	ring.rbegin()->push_back(i);
}

void History::register_args(ProcessStack & stack, int from, int to) {
	// first check if we can push the args
	if (ring.size() > 0 && ring.rbegin()->size() > 0) {
		for (int i = from; i < to; ++i) {
			push_arg(stack[i]);
		}
	}
	// else don't push
}

void History::push_arg(Object::ref arg) {
	ring.rbegin()->rbegin()->args.push_back(arg);
}

void History::leave() {
	if (!ring.empty()) {
		ring.pop_back();
	}
}

// returned list is of type
// ((functon arg1 ... argn) ...)
void History::to_list(Process & proc) {
	ProcessStack & s = proc.stack;
	size_t count = 0; // number of elements in the history
	int sz = ring.size();
	for (outer_ring::iterator i = ring.begin()+sz-1; sz>0; --i, --sz) {
		int sz = i->size();
		for (inner_ring::iterator j = i->begin()+sz-1; sz>0; --j, --sz) {
			// build inner list
			int nelem = 1 + j->args.size(); // +1 for clos
			s.push(j->clos);
			for (std::vector<Object::ref>::iterator k = j->args.begin(); 
					 k != j->args.end(); ++k) {
				s.push(*k);
			}
			s.push(Object::nil());
			for (int i=0; i<nelem; ++i) {
				bytecode_cons(proc, s);
			}
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
			gt->traverse(j->clos);
			// traverse args
			for (std::vector<Object::ref>::iterator k = j->args.begin(); 
					 k != j->args.end(); ++k) {
				gt->traverse(*k);
			}
		}
	}
}
