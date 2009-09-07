#ifndef BYTECODES_H
#define BYTECODES_H
#include "processes.hpp"
#include"types.hpp"
#include"aio.hpp"
#include "symbols.hpp"
#include<boost/shared_ptr.hpp>
#include<string>

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

template<Object::ref (*MF)(void)>
inline void bytecode_(ProcessStack& stack){
	stack.push( (*MF)() );
}

template<Object::ref (*MF)(Process&, Object::ref)>
inline void bytecode_(Process& proc, ProcessStack& stack){
	stack.top() = (*MF)(proc, stack.top());
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_(ProcessStack& stack){
	stack.top() = (*MF)(stack.top());
}

template<Object::ref (*MF)(Process&)>
inline void bytecode_(Process& proc, ProcessStack& stack){
	stack.push( (*MF)(proc) );
}

template<Object::ref const& (*MF)(Process&, Object::ref const&)>
inline void bytecode_(Process& proc, ProcessStack& stack){
	stack.top() = (*MF)(proc, stack.top());
}

template<Object::ref const& (*MF)(Object::ref const&)>
inline void bytecode_(ProcessStack& stack){
	stack.top() = (*MF)(stack.top());
}

template<Object::ref (*MF)(Process&, Object::ref, Object::ref)>
inline void bytecode2_(Process& proc, ProcessStack& stack) {
  Object::ref v2 = stack.top(); stack.pop();
  stack.top() = (*MF)(proc, stack.top(), v2);
}

template<Object::ref (*MF)(Object::ref, Object::ref)>
inline void bytecode2_(ProcessStack& stack){
  Object::ref v2 = stack.top(); stack.pop();
  stack.top() = (*MF)(stack.top(), v2);
}

template<Object::ref (*MF)(Process& proc, Object::ref, Object::ref, Object::ref)>
inline void bytecode3_(Process& proc, ProcessStack& stack) {
  Object::ref v3 = stack.top(); stack.pop();
  Object::ref v2 = stack.top(); stack.pop();
  stack.top() = (*MF)(proc, stack.top(), v2, v3);
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_local_push_(ProcessStack& stack, int N){
	stack.push((*MF)(stack[N]));
}

template<Object::ref const& (*MF)(Object::ref const&)>
inline void bytecode_local_push_(ProcessStack& stack, int N){
	stack.push((*MF)(stack[N]));
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_clos_push_(ProcessStack& stack, Closure& clos, int N){
	stack.push((*MF)(clos[N]));
}

template<Object::ref const& (*MF)(Object::ref const&)>
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

inline void bytecode_bounded(ProcessStack& stack) {
	Object::ref o = stack.top(); stack.pop();
	if (is_a<Symbol*>(o)) {
		Symbol *s = as_a<Symbol*>(o);
		if (s->unbounded()) {
			stack.push(Object::nil());
		} else {
			stack.push(Object::t());
		}
	} else {
		throw_HlError("<bc>bounded expects a symbol on the stack");
	}
}

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
	/*global-set has to retain the stack top! This is because
	<axiom>set has to return the value that was assigned.
	The hl-to-bytecode compiler always assumes bytecodes replace
	all their arguments with a single value; since global-set
	has a single argument, it must be replaced with a single
	value, viz. itself.
	*/
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
inline void bytecode_s_to_sy(Process& proc, ProcessStack& stack) {
	HlString* hls = expect_type<HlString>(stack.top(),
		"'<bc>s-to-sy expects a string"
	);
	std::string s = hls->to_cpp_string();
	Symbol* sy = symbols->lookup(s);
	stack.top() = Object::to_ref(sy);
}
inline void bytecode_sy_to_s(Process& proc, ProcessStack& stack) {
	if(!is_a<Symbol*>(stack.top())) {
		throw_HlError("<bc>sy-to-s expects a symbol");
	}
	Symbol* sy = as_a<Symbol*>(stack.top());
	std::string s = sy->getPrintName();
	HlString::from_cpp_string(proc, stack, s);
}
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

inline void bytecode_tag(Process& proc, ProcessStack& stack){
	/*unlike Arc, we allow the user to tag anything she or he wants.*/
	HlTagged* tp = proc.heap().create<HlTagged>();
	tp->o_type = stack.top(2);
	tp->o_rep = stack.top(1);
	stack.pop();
	stack.top() = Object::to_ref<Generic*>(tp);
}

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

/*integer math*/
inline void bytecode_iplus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = 
        Object::from_a_scaled_int(Object::to_a_scaled_int(a) +  
                                  Object::to_a_scaled_int(b));
      return;
    }
  }
  throw_HlError("'i+ expected two integers");
}

inline void bytecode_iminus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = 
        Object::from_a_scaled_int(Object::to_a_scaled_int(a) -  
                                  Object::to_a_scaled_int(b));
      return;
    }
  }
  throw_HlError("'i- expected two integers");
}

inline void bytecode_imul(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() = Object::to_ref(as_a<int>(a) * as_a<int>(b));
      return;
    }
  }
  throw_HlError("'i* expected two integers");
}

inline void bytecode_idiv(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      int x = as_a<int>(b);
      if (x == 0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(as_a<int>(a) / x);
      return;
    }
  }
  throw_HlError("'i/ expected two integers");
}

inline void bytecode_imod(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      int x = as_a<int>(b);
      if (x == 0)
        throw_HlError("division by zero");
      stack.top() = Object::to_ref(as_a<int>(a) % x);
      return;
    }
  }
  throw_HlError("'imod expects two integers");
}

inline void bytecode_iless(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (is_a<int>(a)) {
    if (is_a<int>(b)) {
      stack.top() =
        as_a<int>(a) < as_a<int>(b) ?          Object::t() :
        /*otherwise*/                          Object::nil() ;
      return;
    }
  }
  throw_HlError("'i< expects two integers");
}

/*float math*/
inline void bytecode_fplus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (maybe_type<Float>(a)) {
    if (maybe_type<Float>(b)) {
      Float* fa = known_type<Float>(a);
      Float* fb = known_type<Float>(b);
      stack.top() = 
        Object::to_ref(Float::mk(p, fa->get() + fb->get()));;
      return;
    }
  }
  throw_HlError("'f+ expected two floats");
}

inline void bytecode_fminus(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (maybe_type<Float>(a)) {
    if (maybe_type<Float>(b)) {
      Float* fa = known_type<Float>(a);
      Float* fb = known_type<Float>(b);
      stack.top() = 
        Object::to_ref(Float::mk(p, fa->get() - fb->get()));;
      return;
    }
  }
  throw_HlError("'f- expected two floats");
}

inline void bytecode_fmul(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (maybe_type<Float>(a)) {
    if (maybe_type<Float>(b)) {
      Float* fa = known_type<Float>(a);
      Float* fb = known_type<Float>(b);
      stack.top() = 
        Object::to_ref(Float::mk(p, fa->get() * fb->get()));;
      return;
    }
  }
  throw_HlError("'f* expected two floats");
}

inline void bytecode_fdiv(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (maybe_type<Float>(a)) {
    if (maybe_type<Float>(b)) {
      Float* fa = known_type<Float>(a);
      Float* fb = known_type<Float>(b);
      if(fb->get() == 0.0)
        throw_HlError("division by zero");
      stack.top() = 
        Object::to_ref(Float::mk(p, fa->get() / fb->get()));;
      return;
    }
  }
  throw_HlError("'f/ expected two floats");
}

inline void bytecode_fless(Process & p, ProcessStack & stack) {
  Object::ref a = stack.top(2);
  Object::ref b = stack.top(); stack.pop();
  if (maybe_type<Float>(a)) {
    if (maybe_type<Float>(b)) {
      Float* fa = known_type<Float>(a);
      Float* fb = known_type<Float>(b);
      stack.top() =
        fa->get() < fb->get() ?                Object::t() :
        /*otherwise*/                          Object::nil() ;
      return;
    }
  }
  throw_HlError("'f< expects two floats");
}

/*consider whether this can be factored out*/
inline void bytecode_string_build(Heap& hp, ProcessStack& stack, int N) {
	HlString::length_create( hp, stack, N );
}
inline void bytecode_string_create(Heap& hp, ProcessStack& stack, int N) {
	HlString::stack_create( hp, stack, N );
}
inline void bytecode_string_sref( Heap& hp, ProcessStack& stack ) {
	HlString::sref(hp, stack );
}

inline void bytecode_char(ProcessStack& stack, int N) {
	stack.push( Object::to_ref(UnicodeChar((uint32_t)N)) );
}

inline void bytecode_i_to_c(ProcessStack & stack) {
	Object::ref i = stack.top(); stack.pop();
	if (!is_a<int>(i)) {
		// TODO: check for bignums (when we have them)
		throw_HlError("i-to-c expects an integer");
	}
	stack.push(Object::to_ref(UnicodeChar((uint32_t)as_a<int>(i))));
}

inline void bytecode_io_pipe(Process& proc, ProcessStack& stack) {
	stack.push(
		Object::to_ref<Generic*>(proc.create<HlIOPort>())
	);
	stack.push(
		Object::to_ref<Generic*>(proc.create<HlIOPort>())
	);
	create_pipe(
		known_type<HlIOPort>(stack.top(2))->p,
		known_type<HlIOPort>(stack.top(1))->p
	);
	bytecode_cons(proc, stack);
}

/*proc-local and err-handler*/
template<Object::ref (Process::*F)>
inline void bytecode_proc_get(Process& proc, ProcessStack& stack) {
	stack.push(proc.*F);
}
template<Object::ref (Process::*F)>
inline void bytecode_proc_set(Process& proc, ProcessStack& stack) {
	proc.*F = stack.top();
}

#endif //BYTECODES_H
