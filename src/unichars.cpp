#include "all_defines.hpp"
#include "processes.hpp"
#include "unichars.hpp"

void UnicodeChar::push_as_int(Process & proc) {
	// !! put an int for the moment
	// !! TODO: use big ints when data doesn't fit in a fixnum
	proc.stack.push(Object::to_ref((int)dat));
}

