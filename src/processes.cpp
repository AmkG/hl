#include"all_defines.hpp"

#include"processes.hpp"

void Process::scan_root_object(GenericTraverser* gt) {
	for(size_t i = 0; i < stack.size(); ++i) {
		gt->traverse(stack[i]);
	}
	/*insert code for traversing cached globals here*/
	/*insert code for traversing process-local vars here*/
}

