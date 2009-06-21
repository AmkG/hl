#include "all_defines.hpp"
#include "history.hpp"
#include "processes.hpp"
#include "bytecodes.hpp"
#include "reader.hpp"

void History::to_list(Process & proc) {
	size_t count = 0; // number of elements in the history
	int sz = ring.size();
	for (outer_ring::iterator i = ring.begin()+sz-1; sz>0; --i, --sz) {
		int sz = i->size();
		for (inner_ring::iterator j = i->begin()+sz-1; sz>0; --j, --sz) {
			std::cerr<<sz<<"\n";
			std::cerr<<*j<<"\n";
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
	std::cerr<<"trav\n";
	int sz = ring.size();
	for (outer_ring::iterator i = ring.begin()+sz-1; sz>0; --i, --sz) {
		int sz = i->size();
		for (inner_ring::iterator j = i->begin()+sz-1; sz>0; --j, --sz) {
			gt->traverse(*j);
		}
	}
}
