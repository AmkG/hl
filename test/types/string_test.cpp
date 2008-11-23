#include"all_defines.hpp"

#include"objects.hpp"
#include"types.hpp"
#include"processes.hpp"
#include"heaps.hpp"
#include"unichars.hpp"

#include<iostream>
#include<cassert>

class TestHeap : public Heap {
protected:
	virtual void scan_root_object(GenericTraverser* gt) {
		for(size_t i = 0; i < stack.size(); ++i) {
			gt->traverse(stack[i]);
		}
	}
public:
	ProcessStack stack;
	explicit TestHeap(size_t sz) : Heap(sz) { }
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
};

void throw_OverBrokenHeart(Generic*) {
	throw OverBrokenHeart();
}

int main(void) {
	TestHeap hp(16 * sizeof(Cons));
	hp.stack.push(Object::to_ref(UnicodeChar('A')));
	hp.stack.push(Object::to_ref(UnicodeChar('m')));
	hp.stack.push(Object::to_ref(UnicodeChar('k')));
	hp.stack.push(Object::to_ref(UnicodeChar('G')));
	HlString::stack_create(hp, hp.stack, 4);

	HlString* Sp = dynamic_cast<HlString*>(as_a<Generic*>(hp.stack.top()));
	assert(Sp);
	assert(Sp->ref(0) == UnicodeChar('A'));
	assert(Sp->ref(1) == UnicodeChar('m'));
	assert(Sp->ref(2) == UnicodeChar('k'));
	assert(Sp->ref(3) == UnicodeChar('G'));

	hp.stack.push(hp.stack.top());
	hp.stack.push(Object::to_ref(UnicodeChar('S')));
	hp.stack.push(Object::to_ref(1));
	HlString::sref(hp, hp.stack);
	assert(hp.stack.top() == Object::to_ref(UnicodeChar('S')));
	hp.stack.pop();

	Sp = dynamic_cast<HlString*>(as_a<Generic*>(hp.stack.top()));
	assert(Sp);
	assert(Sp->ref(0) == UnicodeChar('A'));
	assert(Sp->ref(1) == UnicodeChar('S'));
	assert(Sp->ref(2) == UnicodeChar('k'));
	assert(Sp->ref(3) == UnicodeChar('G'));

	std::cout << "Passed!" << std::endl;
}

