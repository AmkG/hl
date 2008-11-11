#include"all_defines.hpp"
#include"objects.hpp"
#include"heaps.hpp"
#include"types.hpp"

#include<iostream>
#include<cassert>

class MyHeap : public Heap {
protected:
	virtual void scan_root_object(GenericTraverser* gt) {
		gt->traverse(r0);
		gt->traverse(r1);
	}
public:
	Object::ref r0;
	Object::ref r1;
	explicit MyHeap(size_t sz) : Heap(sz) { }
};

class HlError {
public:
	char const* val;
	explicit HlError(char const* nval) : val(nval) { }
};

void throw_HlError(char const* t) {
	throw HlError(t);
}

class OverBrokenHeart {
public:
	Generic* cause;
	explicit OverBrokenHeart(Generic* ncause) : cause(ncause) { }
};

void throw_OverBrokenHeart(Generic* cause) {
	throw OverBrokenHeart(cause);
}

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

struct DeallocError {
};

void throw_DeallocError(void*) {
	throw DeallocError();
}

int main(void) {
	MyHeap tmp(sizeof(Cons));

	std::cout << "allocating 1st" << std::endl;
	tmp.r0 = Object::to_ref<Generic*>(tmp.create<Cons>());
	std::cout << "r0 = @" << as_a<Generic*>(tmp.r0) << std::endl;

	std::cout << "allocating 2nd" << std::endl;
	tmp.r1 = Object::to_ref<Generic*>(tmp.create<Cons>());
	scar(tmp.r1, tmp.r0);
	std::cout << "r0 = @" << as_a<Generic*>(tmp.r0) << std::endl;
	std::cout << "r1 = @" << as_a<Generic*>(tmp.r1) << std::endl;
	assert(car(tmp.r1) == tmp.r0);

	std::cout << "allocating 3rd" << std::endl;
	tmp.r1 = Object::to_ref<Generic*>(tmp.create<Cons>());
	scdr(tmp.r0, tmp.r1);
	std::cout << "r0 = @" << as_a<Generic*>(tmp.r0) << std::endl;
	std::cout << "r1 = @" << as_a<Generic*>(tmp.r1) << std::endl;
	assert(cdr(tmp.r0) == tmp.r1);
}

