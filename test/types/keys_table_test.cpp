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
	ProcessStack& stack = hp.stack;

	/*empty table*/
	stack.push( Object::to_ref<Generic*>(hp.create<HlTable>() ));
	stack.push( stack.top() );
	HlTable::keys(hp, stack);
	assert( !stack.top() );
	stack.pop();

	/*add t key*/
	stack.push( stack[0] );
	stack.push( Object::t() );
	stack.push( Object::t() );
	HlTable::insert(hp, stack);
	stack.pop();
	stack.push( stack.top() );
	/*should contain just one key, t*/
	HlTable::keys(hp, stack);
	assert( stack.top() );
	assert( maybe_type<Cons>(stack.top()) );
	assert( is(Object::t(), car(stack.top())) );
	assert( is(Object::nil(), cdr(stack.top())) );
	stack.pop();

	/*add nil key*/
	stack.push( stack[0] );
	stack.push( Object::t() );
	stack.push( Object::nil() );
	HlTable::insert(hp, stack);
	stack.pop();
	stack.push( stack.top() );
	/*should contain two keys, nil and t, in an unspecified order*/
	HlTable::keys(hp, stack);
	assert( stack.top() );
	assert( maybe_type<Cons>(stack.top()) );
	{Object::ref l2 = cdr(stack.top());
		assert( l2 );
		assert( maybe_type<Cons>(l2));
		assert( !cdr(l2) );
		assert( is(Object::t(), car(stack.top())) || is(Object::t(), car(l2)) );
		assert( is(Object::nil(), car(stack.top())) || is(Object::nil(), car(l2)) );
	}
	stack.pop();

	std::cout << "Passed!" << std::endl;
}

