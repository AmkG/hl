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
		// ?? for some reason, after insertion capacity must be set again
		inner_ring.repeat_set_capacity();
		inner_ring.push_front(Item());

		Item& it = inner_ring.front();
		it.resize(stack.size() - 1, Object::nil());
		it[0] = stack[0];
		/*skip stack[1] in the debug dump*/
		for(size_t i = 1; i < (stack.size() - 1); ++i) {
			it[i] = stack[i + 1];
		}
	}
}

// returned list is of type
// ((functon arg1 ... argn) ...)
void History::to_list(Process & proc) {
	ProcessStack & s = proc.stack;
	size_t count = 0; // number of elements in the history
	Closure* pkclos;
	pkclos = maybe_type<Closure>(stack[1]);
	if(pkclos && pkclos->kontinuation) goto outer_loop;
	pkclos = maybe_type<Closure>(stack[0]);
	if(pkclos && pkclos->kontinuation) goto outer_loop;
	goto end_outer_loop;
outer_loop:
	/*go through the kontinuation*/
	{ InnerRing& ring = *pkclos->kontinuation;
		for(InnerRing::iterator it = ring.begin();
				it != ring.end();
				++it) {
			Item& item = *it;
			{ size_t count = 0;
				for(Item::iterator it = item.begin();
						it != item.end();
						++it) {
					proc.stack.push(*it);
					++count;
				}
				proc.stack.push(Object::nil());
				for(; count; --count) {
					bytecode_cons(proc, stack);
				}
			}
			++count;
		}
	}
	/*search for a parent continuation*/
	for(size_t i = 0; i < pkclos->size(); ++i) {
		Closure* npkclos = maybe_type<Closure>((*pkclos)[i]);
		if(npkclos) {
			if(npkclos->kontinuation) {
				pkclos = npkclos;
				goto outer_loop;
			}
		}
	}
end_outer_loop:
	proc.stack.push(Object::nil());
	// build the list
	for (; count; --count) {
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

