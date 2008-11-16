#include"all_defines.hpp"
#define DEBUG
#include"objects.hpp"
#include<iostream>
#include<cassert>

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

int main (void) {
	Object::ref w = Object::t();
	Object::ref x = Object::nil();
	Object::ref y = Object::t();
	Object::ref z = Object::nil();

	assert( (bool)w );
	assert( !((bool)x) );
	assert( !(!w) );
	assert( !x );
	assert( !(w == x) );
	assert( w != x );
	assert( w == y);
	assert( x == z);
	assert( !(w != y) );
	assert( !(x != z) );
	assert( is_t(w) );
	assert( is_nil(x) );
	assert( !is_nil(w) );
	assert( !is_t(x) );

	Object::ref n = Object::to_ref(1);

	assert( !(w == n));
	assert( !(x == n));
	assert( w != n);
	assert( x != n);

	assert( !is_a<int>(w) );
	assert( !is_a<Generic*>(w) );
	assert( !is_a<Symbol*>(w) );

	assert( !is_a<int>(x) );
	assert( !is_a<Generic*>(x) );
	assert( !is_a<Symbol*>(x) );

	std::cout << "passed!" << std::endl;

	return 0;
}

