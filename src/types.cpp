#include "all_defines.hpp"
#include "types.hpp"

Closure* Closure::NewClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = body;
  c->nonreusable = true;
  c->kontinuation = false;
  return c;
}

Closure* Closure::NewKClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.lifo_create_variadic<Closure>(n);
  c->body = body;
  c->nonreusable = false;
  c->kontinuation = true;
  return c;
}

static void HlString::sref(Heap& hp, ProcessStack& stack) {
	/*first check the inputs*/
	HlString* Tp = expect_type<HlString>(stack.top(3),
				"'string-sref expects a string as first argument");
	if(!is_a<UnicodeChar>(stack.top(2))) {
		throw_HlError("'string-sref expects a character as second argument");
	}
	/*TODO: When BigInts are implemented, consider changing
	this to support BigInts
	*/
	if(!is_a<int>(stack.top(1))) {
		throw_HlError("'string-sref expects an integer as third argument");
	}
	HlStringImpl* Sp = known_type<HlStringImpl>(Tp->impl);
	if(Sp->shared) {
		/*create a new copy!*/
		HlStringImpl* nSp = hp.create_variadic<HlStringImpl>(Sp->size());
		/*revive Tp and Sp*/
		Tp = known_type<HlString>(stack.top(3));
		Sp = known_type<HlStringImpl>(Tp->impl);
		for(size_t i = 0; i < Sp->size(); ++i) {
			(*nSp)[i] = (*Sp)[i];
		}
		Tp->impl = Object::to_ref(nSp);
		Sp = nSp;
	}
	size_t ind = as_a<int>(stack.top(1));
	if(ind >= Sp->size()) {
		throw_HlError("'string-sref index out of bounds");
	}
	(*Sp)[ind] = stack.top(2);
	stack.top(3) = stack.top(2);
	stack.pop(2);
}

static void HlString::stack_create(Heap& hp, ProcessStack& stack, size_t N) {
	/*first, create the implementation*/
	HlStringImpl* Sp = hp.create_variadic<HlStringImpl>(N);
	/*fill in from the stack*/
	for(size_t i = 0; i < N; ++i) {
		if(is_a<UnicodeChar>(stack.top(N-i))) {
			(*Sp)[i] = stack.top(N-i);
		} else {
			throw_HlError("'string bytecode expects characters for all parameters");
		}
	}
	stack.pop(N);
	/*push Sp onto the stack*/
	stack.push(Object::to_ref(Sp));
	/*create the PImpl*/
	HlString* Tp = hp.create<HlString>();
	Tp->impl = stack.top();
	stack.top() = Object::to_ref(Tp);
}

