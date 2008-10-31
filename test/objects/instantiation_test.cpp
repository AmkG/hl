#include"all_defines.hpp"
#define DEBUG
#include"objects.hpp"
#include<iostream>

#include<malloc.h>
#include<stdint.h>

struct Generic {
	char dat;
	Generic(char x) : dat(x) {}
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
	Object::ref x = Object::to_ref(1);
	char* mem = (char*)malloc(256);
	if(mem == 0) {
		std::cout << "failed allocation" << std::endl;
		return 2;
	}

	/*force into alignment*/
	char* for_test;
	if(((intptr_t) mem) & Object::tag_mask != 0) {
		for_test = mem + Object::alignment - (((intptr_t) mem) & Object::tag_mask);
	} else {
		for_test = mem;
	}

	Object::ref y = Object::to_ref(new((void*) for_test) Generic('A'));

	std::cout << "x = " << as_a<int>(x) << std::endl;
	std::cout << "y.dat = " << as_a<Generic*>(y)->dat << std::endl;

	free(mem);
	return 0;
}

