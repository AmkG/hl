#include"all_defines.hpp"
#define DEBUG
#include"objects.hpp"
#include<iostream>

/*This file is EXPECTED to compile with
errors!
*/

struct SomethingTotallyOther {
	char dat;
	SomethingTotallyOther(char x) : dat(x) {}
};

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
	SomethingTotallyOther* p = new SomethingTotallyOther('A');
	Object::ref x = Object::to_ref(p);

	std::cout << "Oops!  I shouldn't have compiled!" << std::endl;

	delete p;
	return 1;
}

