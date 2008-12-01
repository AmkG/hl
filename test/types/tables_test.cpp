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

	/*table creation*/
	stack.push(Object::to_ref(hp.create<HlTable>()));
	assert(stack.top());
	HlTable* tmp = dynamic_cast<HlTable*>(as_a<Generic*>(stack.top()));
	assert(tmp);

	{std::cout << "empty table lookup!" << std::endl;
		stack.push(Object::to_ref(hp.create<Cons>()));
		HlTable& T = *expect_type<HlTable>(stack.top(2), "expected table");
		assert(!T.lookup(Object::nil()));
		assert(!T.lookup(Object::t()));
		assert(!T.lookup(stack.top()));
		assert(!T.lookup(Object::to_ref(0)));
		stack.pop();
	}
	{std::cout << "t key insertion!" << std::endl;
		stack.push(stack.top());	/*table*/
		stack.push(Object::to_ref(0));	/*value*/
		stack.push(Object::t());	/*key*/
		HlTable::insert(hp, stack);
		assert(stack.top() == Object::to_ref(0));
		assert(stack.size() == 2);
		stack.pop();
	}
	{std::cout << "t key lookup!" << std::endl;
		HlTable& T = *expect_type<HlTable>(stack.top(), "expected table");
		assert(T.lookup(Object::t()));
		assert(T.lookup(Object::t()) == Object::to_ref(0));
		assert(!T.lookup(Object::nil()));
	}
	{std::cout << "t key replacement!" << std::endl;
		stack.push(stack.top());	/*table*/
		stack.push(Object::to_ref(1));	/*value*/
		stack.push(Object::t());	/*key*/
		HlTable::insert(hp, stack);
		assert(stack.top() == Object::to_ref(1));
		assert(stack.size() == 2);
		stack.pop();
		HlTable& T = *expect_type<HlTable>(stack.top(), "expected table");
		assert(T.lookup(Object::t()));
		assert(T.lookup(Object::t()) == Object::to_ref(1));
	}
	{std::cout << "multiple key insertion and lookup!" << std::endl;
		size_t NUM_TEST = 32;
		for(size_t i = 0; i < NUM_TEST; ++i) {
			stack.push(stack.top());
			stack.push(Object::to_ref((int) (NUM_TEST - i)));
			stack.push(Object::to_ref((int) i));
			HlTable::insert(hp, stack);
			assert(stack.top() == Object::to_ref((int) (NUM_TEST - i)));
			assert(stack.size() == 2);
			stack.pop();
		}
		HlTable& T = *expect_type<HlTable>(stack.top(), "expected table");
		for(size_t i = 0; i < NUM_TEST; ++i) {
			assert(T.lookup(Object::to_ref((int) i)));
			Object::ref tmp = T.lookup(Object::to_ref((int) i));
			assert(tmp == Object::to_ref((int) (NUM_TEST - i)));
		}
	}

	{std::cout << "string key test!" << std::endl;
		/*key*/
		stack.push(Object::to_ref(UnicodeChar('A')));
		stack.push(Object::to_ref(UnicodeChar('m')));
		stack.push(Object::to_ref(UnicodeChar('k')));
		stack.push(Object::to_ref(UnicodeChar('G')));
		HlString::stack_create(hp, stack, 4);
		/*
			stack[0] = table
			stack[1] = string to test: "AmkG"
		*/
		stack.push(stack[0]);				/*table*/
		stack.push(Object::to_ref(UnicodeChar('X')));	/*value*/
		stack.push(stack[1]);				/*key*/
		HlTable::insert(hp, stack);
		assert(stack.top() == Object::to_ref(UnicodeChar('X')));
		assert(stack.size() == 3);
		stack.pop();

		/*reconstruct the string*/
		stack.push(Object::to_ref(UnicodeChar('A')));
		stack.push(Object::to_ref(UnicodeChar('m')));
		stack.push(Object::to_ref(UnicodeChar('k')));
		stack.push(Object::to_ref(UnicodeChar('G')));
		HlString::stack_create(hp, stack, 4);
		/*
			stack[0] = table
			stack[1] = string to test: "AmkG"
			stack[2] = string to test: "AmkG"
		*/
		{HlTable& T = *known_type<HlTable>(stack[0]);
			assert(T.lookup(stack[2]) == Object::to_ref(UnicodeChar('X')));
		}

		/*modify the first string (the one we used as key)
		and see if the second string still succeeds and
		the first key does not
		*/
		stack.push(stack[1]);
		stack.push(Object::to_ref(UnicodeChar('S')));
		stack.push(Object::to_ref(0));
		HlString::sref(hp, stack);
		assert(stack.top() == Object::to_ref(UnicodeChar('S')));
		stack.pop();
		{HlTable& T = *known_type<HlTable>(stack[0]);
			assert(!T.lookup(stack[1]));
			assert(T.lookup(stack[2]) == Object::to_ref(UnicodeChar('X')));
		}
		/*now check that the strings have all their parts in order*/
		{HlString& S1 = *known_type<HlString>(stack[1]);
		HlString& S2 = *known_type<HlString>(stack[2]);
			assert(S1.ref(0) == UnicodeChar('S'));
			assert(S1.ref(1) == UnicodeChar('m'));
			assert(S1.ref(2) == UnicodeChar('k'));
			assert(S1.ref(3) == UnicodeChar('G'));
			assert(S2.ref(0) == UnicodeChar('A'));
			assert(S2.ref(1) == UnicodeChar('m'));
			assert(S2.ref(2) == UnicodeChar('k'));
			assert(S2.ref(3) == UnicodeChar('G'));
		}
		stack.pop(2);
	}

	stack.restack(0);

	{std::cout << "arrayed table checking!" << std::endl;
		stack.push(Object::to_ref(hp.create<HlTable>()));
		size_t NUM_TEST = 18;
		for(size_t i = 0; i < NUM_TEST; ++i) {
			/*check that it's currently empty*/
			{HlTable& T = *known_type<HlTable>(stack.top());
				assert(!T.lookup(Object::to_ref((int) i)));
			}
			stack.push(stack.top());
			stack.push(Object::to_ref((int) (NUM_TEST - i)));
			stack.push(Object::to_ref((int) i));
			HlTable::insert(hp, stack);
			assert(stack.top() == Object::to_ref((int) (NUM_TEST - i)));
			assert(stack.size() == 2);
			stack.pop();
			/*check each entry*/
			HlTable& T = *known_type<HlTable>(stack.top());
			for(size_t j = 0; j < NUM_TEST; ++j) {
				if(j <= i) {
					Object::ref tmp = T.lookup(Object::to_ref((int) j));
					assert(tmp == Object::to_ref((int) (NUM_TEST - j)));
				} else {
					assert(!T.lookup(Object::to_ref((int) j)));
				}
			}
		}

		/*check insertion of non-numeric key*/
		stack.push(stack.top());
		stack.push(Object::t());
		stack.push(Object::to_ref(UnicodeChar('X')));
		HlTable::insert(hp, stack);
		assert(stack.top() == Object::t());
		assert(stack.size() == 2);
		stack.pop();
		/*now check each entry*/
		HlTable& T = *known_type<HlTable>(stack.top());
		for(size_t i = 0; i < NUM_TEST; ++i) {
			assert(T.lookup(Object::to_ref((int) i)) == Object::to_ref((int) (NUM_TEST - i)));
		}
		assert(T.lookup(Object::to_ref(UnicodeChar('X'))) == Object::t());
	}

	std::cout << "Passed!" << std::endl;
}

