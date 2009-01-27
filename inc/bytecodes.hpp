#ifndef BYTECODES_H
#define BYTECODES_H
#include "processes.hpp"
#include"types.hpp"
#include<boost/shared_ptr.hpp>

/*
By defining the actual bytecode implementation
here, we can (?) possibly make it easier to
change the interpreter system (for example,
direct threading, maybe even dynamic inlining
some day)
*/
/*
RULE: Implementations here are concerned only with
allocating stuff and manipulating stack.  Control
flow and closure creation may very well be coupled
to the interpreter system, so we don't define them
here.
*/

/*---------------------------------------------------------------------------*/

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_(ProcessStack& stack){
	stack.top() = (*MF)(stack.top());
}

template<Object::ref (*MF)(Object::ref, Object::ref)>
inline void bytecode2_(ProcessStack& stack){
  Object::ref v2 = stack.top(); stack.pop();
  stack.top() = (*MF)(stack.top(), v2);
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_local_push_(ProcessStack& stack, int N){
	stack.push((*MF)(stack[N]));
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_clos_push_(ProcessStack& stack, Closure& clos, int N){
	stack.push((*MF)(clos[N]));
}

template<Object::ref (*MF)(Object::ref, Process&)>
inline void bytecode_(Process& proc, ProcessStack& stack){
	stack.top() = (*MF)(stack.top(), proc);
}

template<Object::ref (*MF)(Object::ref, Process&)>
inline void bytecode_local_push_(Process& proc, ProcessStack& stack, int N) {
	stack.push((*MF)(stack[N], proc));
}

template<Object::ref (*MF)(Object::ref, Process&)>
inline void bytecode_clos_push_(Process& proc, ProcessStack& stack,
		Closure& clos, int N){
	stack.push((*MF)(clos[N], proc));
}

/*---------------------------------------------------------------------------*/

/*parameters are on-stack*/
inline void bytecode_cons(Process& proc, ProcessStack& stack){
	Cons* cp = proc.create<Cons>();
	cp->scar(stack.top(2));
	cp->scdr(stack.top(1));
	stack.top(2) = Object::to_ref(cp);
	stack.pop();
}

inline void bytecode_check_vars(ProcessStack& stack, int N){
  if(stack.size() != N)
    throw_HlError("apply: function called with incorrect number of parameters");
}
inline void bytecode_closure_ref(ProcessStack& stack, Closure& clos, int N){
	stack.push(clos[N]);
}
inline void bytecode_global(Process& proc, ProcessStack& stack,
		Symbol *S){
  stack.push(proc.global_read(S));
}
inline void bytecode_int(Process& proc, ProcessStack& stack, int N){
  stack.push(Object::to_ref(N));
}
inline void bytecode_float(Process& proc, ProcessStack& stack, Float *f) {
  // !! WARNING!  Will not work across a GC and if the bytecode
  // !! is executed again.  A GC will *destroy* any traced
  // !! objects (by replacing them with a broken heart tag).
  // !! Since you're pushing "the same" object, you end up
  // !! pushing a broken heart tag, which the hl-side code
  // !! should never see.
  // !! RECOMMENDATION 1: Compose a float using multiple int's, like
  // !! so:
  // !! ((int 1)   ;sign
  // !!  (int 128) ;exponent
  // !!  (int 0)   ;higher mantissa (assume smallint's can't exceed 24 bits)
  // !!  (int 0)   ;lower mantissa  (assume smallint's can't exceed 24 bits)
  // !!  (float-compose))
  // !! RECOMMENDATION 2: *Copy* the float being referred to:
  // !!   Float* nf = Float::mk(proc.heap(), f->get());
  // !!   stack.push(Object::to_ref(nf));
  stack.push(Object::to_ref(f));
}
inline void bytecode_lit_nil(Process&proc, ProcessStack& stack){
	stack.push(Object::nil());
}
inline void bytecode_lit_t(Process&proc, ProcessStack& stack){
	stack.push(Object::t());
}
inline void bytecode_local(ProcessStack& stack, int N){
	stack.push(stack[N]);
}
inline void bytecode_global_set(Process& proc, ProcessStack& stack,
		Symbol *S){
  proc.global_write(S, stack.top());
}
/*
inline void bytecode_sv_set(ProcessStack& stack){
	SharedVar* sp = dynamic_cast<SharedVar*>(stack.top(2));
	if(sp == NULL){
          throw_HlError("container: expected an object of type 'container");
	}
	sp->val = stack.top(1);
	stack.top(2) = stack.top(1);
	stack.pop();
}
*/
inline void bytecode_sym(Process& proc, ProcessStack& stack, Symbol *S) {
  stack.push(Object::to_ref(S));
}
inline void bytecode_symeval(Process& proc, ProcessStack& stack) {
  if(!is_a<Symbol*>(stack.top())) {
    throw_HlError("symeval expects a symbol");
  }
  Symbol* sp = as_a<Symbol*>(stack.top());
  stack.top() = proc.global_read(sp);
}
/*TODO: consider factoring these*/
inline void bytecode_table_create(Process& proc, ProcessStack& stack) {
  stack.push(Object::to_ref(proc.heap().create<HlTable>()));
}
inline void bytecode_table_ref(Process& proc, ProcessStack& stack) {
  HlTable& T = *expect_type<HlTable>(stack.top(2), "table-ref expects a table");
  stack.top(2) = T.lookup(stack.top(1));
  stack.pop();
}
inline void bytecode_table_sref(Process& proc, ProcessStack& stack) {
  HlTable& T = *expect_type<HlTable>(stack.top(3), "table-sref expects a table");
  HlTable::insert(proc.heap(), stack);
}
inline void bytecode_table_keys(Process& proc, ProcessStack& stack) {
  HlTable& T = *expect_type<HlTable>(stack.top(), "table-keys expects a table");
  HlTable::keys(proc.heap(), stack);
}
// inline void bytecode_tag(Process& proc, ProcessStack& stack){
// 	/*have to check that the current type tag isn't
// 	the same as the given type tag
// 	(cref. ac.scm line 801 Anarki, line 654 Arc2)
// 	*/
// 	Generic* ntype = stack.top(2);
// 	Generic* nrep = stack.top(1);
// 	/*determine if rep is built-in type or not
// 	We do this to avoid allocating - 'type on
// 	built-in objects has to allocate the symbol.
// 	*/
// 	Tagged* tp = dynamic_cast<Tagged*>(nrep);
// 	if(tp == NULL){
// 		/*not a tagged type - check atom type instead*/
// 		Symbol* s = dynamic_cast<Symbol*>(ntype);
// 		if(s == NULL){
// 			/*new type isn't a symbol - tag it*/
// 			goto validtag;
// 		} else {
// 			/*check if new tag's atom is the same
// 			as atom type of object
// 			*/
// 			if(s->a == nrep->type_atom()){
// 				goto invalidtag;
// 				/*hot path - most execution routes here*/
// 			} else	goto validtag;
// 		}
// 	/*tagged representation - check if (is ntype (type nrep))*/
// 	} else if(tp->type(proc)->is(ntype)){
// 		goto invalidtag;
// 	} else	goto validtag;
// invalidtag:
// 	/*return representation as-is*/
// 	stack.top(2) = nrep;
// 	stack.pop();
// 	return;
// validtag:
// 	tp = proc.create<Tagged>();
// 	tp->type_o = ntype;
// 	tp->rep_o = nrep;
// 	stack.top(2) = tp;
// 	stack.pop();
// }
inline void bytecode_variadic(Process& proc, ProcessStack& stack, int N){
  int i = stack.size();
  stack.push(Object::nil());
  while(i > N){
    bytecode_cons(proc, stack);
    --i;
  }
  if(i != N)
    throw_HlError("apply: insufficient number of parameters to variadic function");
}

// Math

inline void bytecode_plus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = 
        Object::from_a_scaled_int(Object::to_a_scaled_int(a) +  
                                  Object::to_a_scaled_int(b));
    }
    else {
      Float *f = expect_type<Float>(b, "number expected");
      stack.top() = 
        Object::to_ref(Float::mk(p, as_a<int>(a) + f->get()));
    }
  }
  else {
    Float *f = expect_type<Float>(a, "number expected");
    if (is_a<int>(b)) {
      stack.top() = 
        Object::to_ref(Float::mk(p, f->get() + as_a<int>(b)));
    }
    else {
      Float *f2 = expect_type<Float>(b, "number expected");
      stack.top() = Object::to_ref(Float::mk(p, f->get() + f2->get()));
    }
  }
}

inline void bytecode_minus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = 
        Object::from_a_scaled_int(Object::to_a_scaled_int(a) -  
                                  Object::to_a_scaled_int(b));
    }
    else {
      Float *f = expect_type<Float>(b, "number expected");
      stack.top() = 
        Object::to_ref(Float::mk(p, as_a<int>(a) - f->get()));
    }
  }
  else {
    Float *f = expect_type<Float>(a, "number expected");
    if (is_a<int>(b)) {
      stack.top() = 
        Object::to_ref(Float::mk(p, f->get() - as_a<int>(b)));
    }
    else {
      Float *f2 = expect_type<Float>(b, "number expected");
      stack.top() = Object::to_ref(Float::mk(p, f->get() - f2->get()));
    }
  }
}

inline void bytecode_mul(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = Object::to_ref(as_a<int>(a) * as_a<int>(b));
    }
    else {
      Float *f = expect_type<Float>(b, "number expected");
      stack.top() = Object::to_ref(Float::mk(p, as_a<int>(a) * f->get()));
    }
  }
  else {
    Float *f = expect_type<Float>(a, "number expected");
    if (is_a<int>(b)) {
      stack.top() = Object::to_ref(Float::mk(p, f->get() * as_a<int>(b)));
    }
    else {
      Float *f2 = expect_type<Float>(b, "number expected");
      stack.top() = Object::to_ref(Float::mk(p, f->get() * f2->get()));
    }
  }
}

inline void bytecode_div(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      int x = as_a<int>(b);
      if (x == 0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(as_a<int>(a) / x);
    }
    else {
      Float *f = expect_type<Float>(b, "number expected");
      double x = f->get();
      if (x == 0.0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(Float::mk(p, as_a<int>(a) / x));
    }
  }
  else {
    Float *f = expect_type<Float>(a, "number expected");
    if (is_a<int>(b)) {
      int x = as_a<int>(b);
      if (x == 0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(Float::mk(p, f->get() / x));
    }
    else {
      Float *f2 = expect_type<Float>(b, "number expected");
      double x = f2->get();
      if (x == 0.0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(Float::mk(p, f->get() / x));
    }
  }
}

inline void bytecode_mod(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      int x = as_a<int>(b);
      if (x == 0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(as_a<int>(a) % x);
    }
    else {
      throw_HlError("mod expects two integers");
    }
  }
}

/*consider whether this can be factored out*/
inline void bytecode_string_create(Heap& hp, ProcessStack& stack, int N) {
	HlString::stack_create( hp, stack, N );
}
inline void bytecode_string_sref( Heap& hp, ProcessStack& stack ) {
	HlString::sref(hp, stack );
}

#endif //BYTECODES_H
