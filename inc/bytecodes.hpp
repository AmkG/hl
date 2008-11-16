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

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_local_push_(ProcessStack& stack, int N){
	stack.push((*MF)(stack[N]));
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_clos_push_(ProcessStack& stack, Closure& clos, int N){
	stack.push((*MF)(clos[N]));
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_(Process& proc, ProcessStack& stack){
	stack.top() = (*MF)(stack.top(), proc);
}

template<Object::ref (*MF)(Object::ref)>
inline void bytecode_local_push_(Process& proc, ProcessStack& stack, int N) {
	stack.push((*MF)(stack[N], proc));
}

template<Object::ref (*MF)(Object::ref)>
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

inline void bytecode_car(ProcessStack & stack) {
  stack.top() = car(stack.top());
}

inline void bytecode_cdr(ProcessStack & stack) {
  stack.top() = cdr(stack.top());
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
  //stack.push(proc.get(S));
}
inline void bytecode_int(Process& proc, ProcessStack& stack, int N){
  stack.push(Object::from_a_scaled_int(N));
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
  //proc.assign(S, stack.top());
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
inline void bytecode_symeval(Process& proc, ProcessStack& stack){
	Symbol* sp = expect_type<Symbol>(stack.top(), "symbol expected!");
	//stack.top() = proc.get(sp->a);
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

#endif //BYTECODES_H
