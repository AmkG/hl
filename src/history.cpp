#include "all_defines.hpp"
#include "history.hpp"
#include "processes.hpp"
#include "bytecodes.hpp"

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
