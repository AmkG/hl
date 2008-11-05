#include"all_defines.hpp"
#define DEBUG
#include"objects.hpp"
#include"unichars.hpp"

#include<iostream>
#include<cassert>
#include<stdint.h>

struct RangeError {
};
struct TypeError {
};

void throw_RangeError(const char*) {
	throw RangeError();
}
void throw_TypeError(Object::ref, const char*) {
	throw TypeError();
}

int main(void) {
	Object::ref x;
	x = Object::to_ref(UnicodeChar('A'));

	assert(is_a<UnicodeChar>(x));
	assert(!is_a<int>(x));
	assert(!is_a<Generic*>(x));
	assert(!is_a<Symbol*>(x));
	assert(as_a<UnicodeChar>(x).dat == 'A');

	std::cout << "passed" << std::endl;
	return 0;
}

