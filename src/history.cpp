#include "all_defines.hpp"
#include "history.hpp"
#include "processes.hpp"
#include "bytecodes.hpp"
#include "reader.hpp"
#include "types.hpp"

void History::entry(void) {
	if(known_type<Closure>(stack[0])->kontinuation) {
		/*called a continuation: remove tail calls using this continuation*/
		known_type<Closure>(stack[0])->kontinuation->clear();
	} else {
		/*called an ordinary function: add to tail calls on current continuation*/
		if(stack.size() < 2) return;

		Closure* pkclos = maybe_type<Closure>(stack[1]);
		if(!pkclos) return;

		Closure& kclos = *pkclos;
		if(!kclos.kontinuation) return;

		InnerRing& inner_ring = *kclos.kontinuation;
		inner_ring.push_back(Item());
		// ?? for some reason, after insertion capacity must be set again
		inner_ring.repeat_set_capacity();

		Item& it = inner_ring.back();
		it.resize(stack.size() - 1);
		it[0] = stack[0];
		/*skip stack[1] in the debug dump*/
		for(size_t i = 1; i < stack.size(); ++i) {
			it[i] = stack[i + 1];
		}
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

void History::InnerRing::traverse_references(GenericTraverser* gt) {
	for(iterator it = begin(); it != end(); ++it) {
		for(Item::iterator vit = it->begin(); vit != it->end(); ++vit) {
			gt->traverse(*vit);
		}
	}
}

