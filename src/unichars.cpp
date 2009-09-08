#include "all_defines.hpp"
#include "processes.hpp"
#include "unichars.hpp"

void UnicodeChar::push_as_int(Process & proc) {
	// !! put an int for the moment
	// !! TODO: use big ints when data doesn't fit in a fixnum
	// ?? wait a minute: characters are currently specc'ed
	// ?? as being unsigned integral types within the 0-> 2**21-1
	// ?? range (i.e. Unicode code point).  In fact, the spec
	// ?? requires for fixnums to be at least -2**23 -> 2**23-1
	// ?? http://hl-language.sourceforge.net/legit-numbers.html
	// ?? so characters will always fit in fixnum's
	proc.stack.push(Object::to_ref((int)dat));
}

