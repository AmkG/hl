#include"all_defines.hpp"
#include <stdlib.h> // for size_t
#include <string>
#include <sstream>
#include "types.hpp"
#include "executors.hpp"
#include "bytecodes.hpp"

#ifdef DEBUG
  #include <typeinfo>
  #include <iostream>
#endif

ExecutorTable Executor::tbl;

// map opcodes to bytecodes
static std::map<Symbol*,_bytecode_label> bytetb;

static _bytecode_label bytecodelookup(Symbol *s){
	std::map<Symbol*, _bytecode_label>::iterator i = bytetb.find(s);
	if(i != bytetb.end()){
		return i->second;
	} else {
          std::string err = "assemble: unknown bytecode form: ";
          err += s->getPrintName();
          throw_HlError(err.c_str());
	}
}

template<class E>
static inline Executor* THE_EXECUTOR(void) {
	return new E();
}

class InitialAssignments {
public:
	InitialAssignments const& operator()(
			char const* s,
			_bytecode_label l) const{
		bytetb[symbols->lookup(s)] = l;
		return *this;
	}
	InitialAssignments const& operator()(
			char const* s,
			Executor* e) const {
		Executor::reg(symbols->lookup(s), e);
		return *this;
	}
};

class IsSymbolPackaged : public Executor {
public:
	bool run(Process& proc, size_t& reductions) {
		/*given:
			stack[0] = unused
			stack[1] = k
			stack[2] = a symbol
		calls k with Object::t() if symbol is
		packaged, Object::nil() otherwise
		*/
		ProcessStack& stack = proc.stack;
		Object::ref arg = stack[2];
		stack[2] = Object::nil();
		if(is_a<Symbol*>(arg)) {
			Symbol& S = *as_a<Symbol*>(arg);
			std::string txt = S.getPrintName();
			if(txt[0] == '<') {
				/*scan for a matching >
				symbol print names are in UTF-8, so
				being ignorant of the encodation is
				safe
				*/
				for(size_t i = 1; i < txt.size(); ++i) {
					if(txt[i] == '>') {
						stack[2] = Object::t();
						break;
					}
				}
			}
		}
		stack.restack(2);
		return true;
	}
};

void Bytecode::push(bytecode_t b) {
  if (code==NULL) {
    code = new bytecode_t[64]; // default initial size
    nextCode = 0;
    codeSize = 64;
  } else if (nextCode >= codeSize) {
    bytecode_t *nb = new bytecode_t[codeSize*2];
    for (size_t i = 0; i<codeSize; i++)
      nb[i] = code[i];
    delete [] code;
    code = nb;
    codeSize *= 2;
  }
  code[nextCode++] = b;
}

void Bytecode::push(_bytecode_label op, intptr_t val) {
  push((bytecode_t){op, val});
}

void Bytecode::push(Symbol *s, intptr_t val) {
  push(bytecodelookup(s), val);
}

void Bytecode::push(const char *s, intptr_t val) {
  push(symbols->lookup(s), val);
}

// generic closure/k_closure/k_closure_recreate/k_closure_reuse 
// assembly operation
class GenClosureAs : public AsOp {
private:
  const char *to_build;
public:
  GenClosureAs(const char *to_build) : to_build(to_build) {}
  void assemble(Process & proc);
};

void GenClosureAs::assemble(Process & proc) {
  // assemble the seq. arg, which must be on the top of the stack
  assembler.go(proc);
  Object::ref res = proc.stack.top(); proc.stack.pop();
  Object::ref arg = proc.stack.top(); proc.stack.pop();
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  size_t iconst = b->closeOver(res); // close over the body
  // reference to the body
  b->push("const-ref", iconst);
  // build the closure
  b->push(to_build, as_a<int>(arg));
};

class ClosureAs : public GenClosureAs {
public:
  ClosureAs() : GenClosureAs("build-closure") {}
};

class KClosureAs : public GenClosureAs {
public:
  KClosureAs() : GenClosureAs("build-k-closure") {}
};

class KClosureRecreateAs : public GenClosureAs {
public:
  KClosureRecreateAs() : GenClosureAs("build-k-closure-recreate") {}
};

class KClosureReuseAs : public GenClosureAs {
public:
  KClosureReuseAs() : GenClosureAs("build-k-closure-reuse") {}
};

// assemble instructions to push a complex constant on the stack
// a complex constant is one that resides on the process-local heap
class ComplexAs : public AsOp {
public:
  void assemble(Process & proc);
};

void ComplexAs::assemble(Process & proc) {
  proc.stack.pop(); // there should be no sequence
  Object::ref arg = proc.stack.top(); proc.stack.pop();
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  if (Assembler::isComplexConst(arg)) {
    size_t i = b->closeOver(arg);
    // generate a reference to it
    b->push("const-ref", i);
  } else {
    throw_HlError("assemble: const expects a complex arg");
  }
}

// build a jmp-if instruction out of a (if (...) (...)) op
class IfAs : public AsOp {
public:
  void assemble(Process & proc);
};

void IfAs::assemble(Process & proc) {
  Object::ref seq = proc.stack.top(); proc.stack.pop();
  proc.stack.pop(); // throw away simple arg
  size_t to_skip = as_a<int>(expect_type<Cons>(seq)->len());
  // skipping instruction
  expect_type<Bytecode>(proc.stack.top())->push("jmp-if", to_skip);
  // append seq with seq being assembled
  Object::ref tail = seq;
  while (cdr(tail)!=Object::nil()) // search the tail
    tail = cdr(tail);
  // stack now is
  //  - current bytecode
  //  - seq being assembled
  scdr(tail, proc.stack.top(2));
  proc.stack.top(2) = seq;
  // now main driver will continue and assemble the if body and then 
  // the rest of the original sequence
}

void Assembler::go(Process & proc) {
  Bytecode *b = proc.create_variadic<Bytecode>(countConsts(proc.stack.top()));
  proc.stack.push(Object::to_ref(b));

  // stack now is:
  // - current Bytecode
  // - seq to assemble

  while (proc.stack.top(2)!=Object::nil()) {
    // push the arguments
    Object::ref current_op = car(proc.stack.top(2));
    proc.stack.top(2) = cdr(proc.stack.top(2)); // advance
    size_t len = as_a<int>(expect_type<Cons>(current_op)->len());
    // push the current args on the stack
    switch (len) {
    case 0: // no args
      proc.stack.push(Object::nil());
      proc.stack.push(Object::nil());
      break;
    case 1: 
      if (maybe_type<Cons>(car(cdr(current_op)))) { // seq
        proc.stack.push(Object::nil()); // empty simple arg
        proc.stack.push(car(cdr(current_op)));
      } else {
        proc.stack.push(car(cdr(current_op)));
        proc.stack.push(Object::nil());
      }
      break;
    case 2:
      proc.stack.push(car(cdr(current_op))); // simple
      proc.stack.push(car(cdr(cdr(current_op)))); // seq
      break;
    default:
      throw_HlError("assemble: wrong number of arguments");
    }
    // stack now is:
    // - seq arg
    // - simple arg
    // - current Bytecode
    // - seq being assembled
    if (!is_a<Symbol*>(car(current_op)))
      throw_HlError("assemble: symbol expected in operator position");
    Symbol *op = as_a<Symbol*>(car(current_op));
    if (tbl.find(op)!=tbl.end()) {
      // do the call
      tbl[op]->assemble(proc);
    } else {
      // default behavior:
      // ignore sequence, extracts the simple argument and lookup
      // an interpretable bytecode
      proc.stack.pop(); // throw away sequence
      Object::ref arg = proc.stack.top(); proc.stack.pop();
      if (isComplexConst(arg))
        throw_HlError("assemble: complex arg found where simple expected");
      Bytecode *b = expect_type<Bytecode>(proc.stack.top());
      b->push(op, simpleVal(arg));
    }
  }
}

intptr_t Assembler::simpleVal(Object::ref sa) {
  if (is_a<int>(sa))
    return as_a<int>(sa);
  else {
    if (is_a<Symbol*>(sa))
      return (intptr_t)(as_a<Symbol*>(sa));
    else // it's a Generic*
      return (intptr_t)(as_a<Generic*>(sa));
  }
}

bool Assembler::isComplexConst(Object::ref obj) {
  return maybe_type<Cons>(obj) || maybe_type<Float>(obj);
}

// count the number of complex constants 
size_t Assembler::countConsts(Object::ref seq) {
  size_t n = 0;
  for(Object::ref i = seq; i!=Object::nil(); i = cdr(i)) {
    for (Object::ref i2 = car(i); i2!=Object::nil(); i2 = cdr(i2)) {
      if (isComplexConst(car(i2)))
        n++;
    }
  }

  return n;
}

// assemble from a string representation
Object::ref inline_assemble(Process & proc, const char *code) {
  std::stringstream code_stream(code);
  read_sequence(proc, code_stream); // leaves seq in the stack
  assembler.go(proc); // take seq from the stack
  Object::ref res = proc.stack.top(); proc.stack.pop();
  return res;
}

/*attempts to deallocate the specified object if it's a reusable
continuation closure
*/
static void attempt_kclos_dealloc(Heap& hp, Generic* gp) {
	Closure* kp = dynamic_cast<Closure*>(gp);
	if(kp == NULL) return;
	if(!kp->reusable()) return;
	hp.lifo_dealloc(gp);
}

#define SETCLOS(name) name = dynamic_cast<Closure*>(as_a<Generic*>(stack[0]))

ProcessStatus execute(Process& proc, size_t& reductions, Process*& Q, bool init){
  /*REMINDER
    All allocations of Generic objects on the
    Process proc *will* invalidate any pointers
    and references to Generic objects in that
    process.
    i.e. proc.heap().create<Anything>() will *always*
    invalidate any existing pointers.
    (technically it will only do so if a GC
    is triggered but hey, burned once, burned
    for all eternity)
  */

  ProcessStack & stack = proc.stack;
  if (init) {
    /*So why isn't this, say, a separate function?  Well,
      because I originally had this impression that labels
      were inaccessible outside of the function where they
      were defined, and now I'm too lazy to move it out of
      here
    */
    InitialAssignments()
      /*built-in functions accessible via $*/
      /*bytecodes*/
      ("apply",		THE_BYTECODE_LABEL(apply))
      ("apply-invert-k",	THE_BYTECODE_LABEL(apply_invert_k))
      ("apply-k-release",	THE_BYTECODE_LABEL(apply_k_release))
      ("apply-list",		THE_BYTECODE_LABEL(apply_list))
      ("build-closure", THE_BYTECODE_LABEL(build_closure))
      ("build-k-closure",		THE_BYTECODE_LABEL(build_k_closure))
      ("build-k-closure-recreate",THE_BYTECODE_LABEL(build_k_closure_recreate))
      ("build-k-closure-reuse",	THE_BYTECODE_LABEL(build_k_closure_reuse))
      ("car",			THE_BYTECODE_LABEL(car))
      ("scar",                  THE_BYTECODE_LABEL(scar))
      ("car-local-push",	THE_BYTECODE_LABEL(car_local_push))
      ("car-clos-push",	THE_BYTECODE_LABEL(car_clos_push))
      ("cdr",			THE_BYTECODE_LABEL(cdr))
      ("scdr",                  THE_BYTECODE_LABEL(scdr))
      ("cdr-local-push",	THE_BYTECODE_LABEL(cdr_local_push))
      ("cdr-clos-push",	THE_BYTECODE_LABEL(cdr_clos_push))
      ("check-vars",		THE_BYTECODE_LABEL(check_vars))
      ("closure-ref",		THE_BYTECODE_LABEL(closure_ref))
      ("composeo",		THE_BYTECODE_LABEL(composeo))
      ("composeo-continuation",	THE_BYTECODE_LABEL(composeo_continuation))
      ("cons",		THE_BYTECODE_LABEL(cons))
      ("const-ref",     THE_BYTECODE_LABEL(const_ref))
      ("continue",		THE_BYTECODE_LABEL(b_continue))
      ("continue-local",	THE_BYTECODE_LABEL(continue_local))
      ("continue-on-clos",	THE_BYTECODE_LABEL(continue_on_clos))
      ("global",		THE_BYTECODE_LABEL(global))
      ("global-set",		THE_BYTECODE_LABEL(global_set))
      ("halt",		THE_BYTECODE_LABEL(halt))
      ("halt-local-push",	THE_BYTECODE_LABEL(halt_local_push))
      ("halt-clos-push",	THE_BYTECODE_LABEL(halt_clos_push))
      ("jmp-nil",			THE_BYTECODE_LABEL(jmp_nil))
      //("if-local",		THE_BYTECODE_LABEL(if_local))
      ("int",			THE_BYTECODE_LABEL(b_int))
      ("float",                 THE_BYTECODE_LABEL(b_float))
      ("ccc", THE_BYTECODE_LABEL(ccc))
      ("lit-nil",		THE_BYTECODE_LABEL(lit_nil))
      ("lit-t",		THE_BYTECODE_LABEL(lit_t))
      ("local",		THE_BYTECODE_LABEL(local))
      ("monomethod",		THE_BYTECODE_LABEL(monomethod))
      ("reducto",		THE_BYTECODE_LABEL(reducto))
      ("reducto-continuation",   THE_BYTECODE_LABEL(reducto_continuation))
//       ("rep",			THE_BYTECODE_LABEL(rep))
//       ("rep-local-push",	THE_BYTECODE_LABEL(rep_local_push))
//       ("rep-clos-push",	THE_BYTECODE_LABEL(rep_clos_push))
      ("sv",			THE_BYTECODE_LABEL(sv))
      ("sv-local-push",	THE_BYTECODE_LABEL(sv_local_push))
      ("sv-clos-push",	THE_BYTECODE_LABEL(sv_clos_push))
      ("sv-ref",		THE_BYTECODE_LABEL(sv_ref))
      ("sv-ref-local-push",	THE_BYTECODE_LABEL(sv_ref_local_push))
      ("sv-ref-clos-push",	THE_BYTECODE_LABEL(sv_ref_clos_push))
      ("sv-set",		THE_BYTECODE_LABEL(sv_set))
      ("sym",			THE_BYTECODE_LABEL(sym))
      ("symeval",		THE_BYTECODE_LABEL(symeval))
      ("table-create",		THE_BYTECODE_LABEL(table_create))
      ("table-ref",		THE_BYTECODE_LABEL(table_ref))
      ("table-sref",		THE_BYTECODE_LABEL(table_sref))
      ("table-map",		THE_BYTECODE_LABEL(table_map))
//       ("tag",			THE_BYTECODE_LABEL(tag))
//       ("type",		THE_BYTECODE_LABEL(type))
//       ("type-local-push",	THE_BYTECODE_LABEL(type_local_push))
//       ("type-clos-push",	THE_BYTECODE_LABEL(type_clos_push))
      ("variadic",		THE_BYTECODE_LABEL(variadic))
      ("do-executor",               THE_BYTECODE_LABEL(do_executor))
      ("+",                     THE_BYTECODE_LABEL(plus))
      ("-",                     THE_BYTECODE_LABEL(minus))
      ("*",                     THE_BYTECODE_LABEL(mul))
      ("/",                     THE_BYTECODE_LABEL(div))
      ("mod",                     THE_BYTECODE_LABEL(mod))
      /*declare executors*/
      ("is-symbol-packaged",	THE_EXECUTOR<IsSymbolPackaged>())
      /*assign bultin global*/
      ;/*end initializer*/

    // initialize assembler operations
    assembler.reg<ClosureAs>(symbols->lookup("closure"));
    assembler.reg<KClosureAs>(symbols->lookup("k-closure"));
    assembler.reg<KClosureRecreateAs>(symbols->lookup("k-closure-recreate"));
    assembler.reg<KClosureReuseAs>(symbols->lookup("k-closure-reuse"));
    assembler.reg<IfAs>(symbols->lookup("if"));

    /*
     * build and assemble various bytecode sequences
     * these will hold a fixed Bytecodes that will be used
     * as the continuations for various bytecodes
     */
    symbols->lookup("$reducto-cont-body")->
      set_value(inline_assemble(proc, "(reducto-continuation) (continue)"));
    symbols->lookup("$ccc-fn-body")->
      set_value(inline_assemble(proc, "(check-vars 3) (continue-on-clos 0)"));
    symbols->lookup("$composeo-cont-body")->
      set_value(inline_assemble(proc, "(composeo-continuation ) (continue )"));

    return process_running;
  }
  // main VM loop
 call_current_closure:
  if(--reductions == 0) return process_running;
  // get current closure
#ifdef DEBUG
  if (is_a<int>(stack[0]))
    std::cerr << "Type on stacktop: int" << std::endl;
  else {
    std::type_info const &inf = typeid(as_a<Generic*>(stack[0]));
    std::cerr << "Type on stacktop: " << inf.name() << std::endl;
  }
#endif
  Closure *clos = expect_type<Closure>(stack[0], "execute: closure expected!");
  // to start, call the closure in stack[0]
  DISPATCH_BYTECODES {
    BYTECODE(apply): {
      INTPARAM(N);
      stack.restack(N);
      goto call_current_closure;
    } NEXT_BYTECODE;
    /*Used by a continuation that would
      like to reuse its closure if possible.
      It can't reuse its closure too early
      because the arguments to the next
      function are likely to depend on the
      closure entries, so it must first
      compute the arguments, then compute
      the values for closure, then finally
      construct the closure.  This means
      that the continuation gets pushed
      last; this bytecode inverts the k
    */
    BYTECODE(apply_invert_k): {
      INTPARAM(N);
      Object::ref k = stack.top(); stack.pop();
      stack.top(N-1) = k;
      stack.restack(N);
      goto call_current_closure;
    } /***/ NEXT_BYTECODE; /***/
    /*Used by a continuation that will
      perform an otherwise ordinary call;
      this tries to release the current
      closure if possible
    */
    BYTECODE(apply_k_release): {
      INTPARAM(N);
      /*TODO: insert debug checking for is_a<Generic*> here*/
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      stack.restack(N);
      goto call_current_closure;
    } /***/ NEXT_BYTECODE; /***/
    BYTECODE(apply_list): {
      Object::ref tmp;
      stack.restack(3);
      /*destructure until stack top is nil*/
      while(stack.top()!=Object::nil()){
        tmp = stack.top();
        bytecode_car(stack);
        // we don't expect car to
        // allocate, so tmp should
        // still be valid
        stack.push(tmp);
        bytecode_cdr(stack);
      }
      stack.pop();
      goto call_current_closure;
    } /***/ NEXT_BYTECODE; /***/
    BYTECODE(build_closure): {
      INTPARAM(N);
      Closure* nclos = Closure::NewClosure(proc, N);
      nclos->codereset(stack.top()); stack.pop();
      SETCLOS(clos); // allocation may invalidate clos
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(build_k_closure): {
    k_closure_perform_create:
      INTPARAM(N);
      Closure *nclos = Closure::NewKClosure(proc, N);
      nclos->codereset(stack.top()); stack.pop();
      SETCLOS(clos);
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(build_k_closure_recreate): {
      /*TODO: insert debug checking for is_a<Generic*> here*/
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      /*put a random object in stack[0]*/
      stack[0] = stack[1];
      /****/ goto k_closure_perform_create; /****/
    } NEXT_BYTECODE;
    /*attempts to reuse the current
      continuation closure.  Falls back
      to allocating a new KClosure if
      the closure isn't a continuation
      (for example due to serialization)
      or if it can't be reused (due to
      'ccc).
    */
    BYTECODE(build_k_closure_reuse): {
      INTPARAM(N);
      Closure *nclos = expect_type<Closure>(stack[0], "Closure expected!");
      if(!nclos->reusable()) {
        // Use the size of the current closure
        nclos = Closure::NewKClosure(proc, clos->size());
        nclos->codereset(stack.top()); stack.pop();
        //clos is now invalid
        SETCLOS(clos);
      } else {
        nclos->codereset(stack.top()); stack.pop();
      }
      for(int i = N; i ; --i){
        // !! closure size may be different !! -- stefano
        // ?? It's OK: the compiler will not generate
        // ?? this bytecode at all if the current closure
        // ?? is known to be smaller.  Basically there's a
        // ?? decision in the compiler's bytecode generator
        // ?? which chooses between k-closure-reuse and
        // ?? k-closure-recreate.  k-closure-reuse is emitted
        // ?? if the current closure is larger or equal size;
        // ?? k-closure-recreate is emitted if the current
        // ?? closure is smaller.  cref:
        // ?? snap/arc2b/bytecodegen.arc:156 -- almkglor
        // ?? Note2: probably we should insert some sort
        // ?? of checking in DEBUG mode -- almkglor
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(car): {
      /*exact implementation is in
        inc/bytecodes.hpp ; we can
        actually change the interpreter
        system.
      */
      bytecode_<&car>(stack);
    } NEXT_BYTECODE;
    BYTECODE(scar): {
      bytecode2_<&scar>(stack);
    } NEXT_BYTECODE;
    BYTECODE(car_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&car>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(car_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&car>(stack, *clos, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr): {
      bytecode_cdr(stack);
    } NEXT_BYTECODE;
    BYTECODE(scdr): {
      bytecode2_<&scdr>(stack);
    } NEXT_BYTECODE;
    BYTECODE(cdr_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&cdr>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&cdr>(stack, *clos, N);
    } NEXT_BYTECODE;
    BYTECODE(check_vars): {
      INTPARAM(N);
      bytecode_check_vars(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(closure_ref): {
      INTPARAM(N);
      bytecode_closure_ref(stack, *clos, N);
    } NEXT_BYTECODE;
    BYTECODE(composeo): {
      /*destructure closure*/
      stack.push((*clos)[0]);
      stack[0] = (*clos)[1];
      Closure& kclos = *Closure::NewKClosure(proc, 2); 
      kclos.codereset(proc.global_read(symbols->lookup("$composeo-cont-body")));
      // clos is now invalid
      /*continuation*/
      kclos[0] = stack[1];
      /*next function*/
      kclos[1] = stack.top(); stack.pop();
      stack[1] = Object::to_ref(&kclos);
      goto call_current_closure; // this will revalidate clos
    } NEXT_BYTECODE;
    BYTECODE(composeo_continuation): {
      stack.push((*clos)[1]);
      stack.push((*clos)[0]);
      stack.push(stack[1]);
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      stack.restack(3);
      goto call_current_closure;
    } NEXT_BYTECODE;
    BYTECODE(cons): {
      bytecode_cons(proc,stack);
      SETCLOS(clos);
    } NEXT_BYTECODE;
    BYTECODE(const_ref): {
      INTPARAM(i);
      // !! too many indirections and conversions
      // !! consider holding a reference to the current Bytecode*
      Bytecode &b = *expect_type<Bytecode>(clos->code());
      proc.stack.push(b[i]);
    } NEXT_BYTECODE;
    /*
      implements the common case
      where we just return a value
      to our continuation.
    */
    /* "continue" might conflict with C++ keyword */
    BYTECODE(b_continue): {
      stack.top(2) = stack[1];
      stack.restack(2);
      goto call_current_closure;
    } NEXT_BYTECODE;
    /*
      implements the case where we
      just return a local variable
      (probably computed by a
      primitive) to our continuation
    */
    BYTECODE(continue_local): {
      INTPARAM(N);
      stack.push(stack[N]);
      stack.top(2) = stack[1];
      stack.restack(2);
      goto call_current_closure;
    } NEXT_BYTECODE;
    /*
      handles the case where the
      continuation we want to return
      to is in our own closure.
    */
    BYTECODE(continue_on_clos): {
      INTPARAM(N);
      Object::ref gp = stack.top();
      stack.top() = (*clos)[N];
      stack.push(gp);
      /*TODO: insert debug checking for is_a<Generic*> here*/
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      stack.restack(2);
      goto call_current_closure;
    } NEXT_BYTECODE;
    BYTECODE(global): {
      SYMPARAM(S);
      bytecode_global(proc, stack, S);
    } NEXT_BYTECODE;
    BYTECODE(global_set): {
      SYMPARAM(S);
      bytecode_global_set(proc,stack,S);
    } NEXT_BYTECODE;
    BYTECODE(halt): {
      stack.restack(1);
      return process_dead;
    } NEXT_BYTECODE;
    BYTECODE(halt_local_push): {
      INTPARAM(N);
      stack.push(stack[N]);
      stack.restack(1);
      return process_dead;
    } NEXT_BYTECODE;
    BYTECODE(halt_clos_push): {
      INTPARAM(N);
      stack.push((*clos)[N]);
      stack.restack(1);
      return process_dead;
    } NEXT_BYTECODE;
    BYTECODE(jmp_nil): {
      INTPARAM(N); // number of operations to skip
      Object::ref gp = stack.top(); stack.pop();
      if (gp==Object::nil()) { // jump if false
        pc += N;
        // NEXT_BYTECODE will increment
      }
    } NEXT_BYTECODE;
    /*
      we expect this to be much more
      common due to CPS conversion; in
      fact we don't expect a plain 'if
      bytecode at all...
    */
    //BYTECODE(if_local): {
    //INTSEQPARAM(N,S);
    //if(stack[N]!=Object::nil()){
    //  pc = S;
    //  pc--; // NEXT_BYTECODE will increment
    //}
    //} NEXT_BYTECODE;
    BYTECODE(b_int): {
      INTPARAM(N);
      bytecode_int(proc, stack, N);
    } NEXT_BYTECODE;
    BYTECODE(b_float): {
      FLOATPARAM(f);
      bytecode_float(proc, stack, f);
    } NEXT_BYTECODE;
    BYTECODE(ccc): {

      // !!! WARNING !!!
      // This is *not* compatible with the SNAP 'ccc bytecode.
      // In particular, this passes the continuation *directly*.
      // However, ALL HL-SIDE FUNCTIONS NEVER EXPECT A
      // CONTINUATION.  INSTEAD, THEY EXPECT A "REAL"
      // FUNCTION.
      // In particular, a continuation accepts EXACTLY TWO
      // parameters: itself, and the return value.  A "REAL"
      // function accepts AT LEAST TWO: itself, a continuation,
      // plus any parameters.  The two types of functions
      // conflict on the second parameter: continuations
      // expect a return value, real functions accept a
      // continuation.
      // In particular, the function in this form:
      //   (ccc
      //     (fn (k)
      //       (k 0)))
      // compiles down to:
      //   (closure 0
      //     (check-vars 3)
      //     (local 2)
      //     (local 1)
      //     (int 0)
      //     (apply 3))
      // Notice that it gives k *3* parameters: k, its
      // own continuation, and the int 0.
      // However if we use the ccc bytecode for this, the
      // function k will expect a single parameter, the
      // return value.
      // Please review the code in snap/src/executors.cpp
      //
      // In particular, the hl-side ccc will have to be
      // assembled from:
      //   (closure 0
      //     (check-vars 3)
      //     (closure 0
      //       (ccc))
      //     (local 1)
      //     (local 2)
      //     (closure 1
      //       (closure-ref 0)  ; f
      //       (local 1)        ; k
      //       (local 1)
      //       (closure 1       ; the k we pass to the function
      //         (check-vars 3)
      //         ; actually performs the call to the
      //         ; continuation
      //         (closure-ref 0)
      //         (local 2)
      //         (apply 2)))
      //     (apply 3))
      //   (global-set ccc)
      // Thus this bytecode is currently unuseable except
      // to mark that continuations cannot be used; 'ccc
      // should do something *equivalent* to:
      //   (fn (k f)
      //     (f k
      //        (fn (_ignored_k val)
      //          (k f))))
      // !!! WARNING !!!

      // expect current continuation and function on the stack
      Closure *k = expect_type<Closure>(stack[1], "ccc expects a continuation");
      Closure *f = expect_type<Closure>(stack[2], "ccc expects a closure");
      k->banreuse(); // continuation can't be reused
      // now call f
      stack[0] = Object::to_ref(f);
      // stack[1] already holds current continuation
      Closure *arg = Closure::NewClosure(proc, 1);
      arg->codereset(proc.global_read(symbols->lookup("$ccc-fn-body")));
      (*arg)[0] = Object::to_ref(k); // close other current continuation
      stack[2] = Object::to_ref(arg);
      //(f current-continuation function-that-will-call-current-continuation)
      goto call_current_closure; // do the call
    } NEXT_BYTECODE;
    BYTECODE(lit_nil): {
      bytecode_lit_nil(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(lit_t): {
      bytecode_lit_t(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(local): {
      INTPARAM(N);
      bytecode_local(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(monomethod): {
      HlTable& T = *known_type<HlTable>((*clos)[0]);
      if(stack.size() >= 3) {
         Object::ref tp = type(stack[2]);
         Object::ref f = T.lookup(tp);
         if(f) stack[0] = f;
         else  stack[0] = T.lookup(Object::nil());
      } else {
         stack[0] = T.lookup(Object::nil());
      }
      goto call_current_closure;
    } NEXT_BYTECODE;
    /*
      reducto is a bytecode to *efficiently*
      implement the common reduction functions,
      such as '+.  This bytecode avoids
      allocating new space when called with 2
      arguments or less, and only allocates
      a reusable continuation closure that is
      used an array otherwise
      See also the executor reducto_continuation.
    */
    BYTECODE(reducto): {
      /*determine #params*/
      /*we have two hidden arguments,
        the current closure and the
        current continuation
        This shouldn't be callable without
        at least those two parameters,
        but better paranoid than sorry
        (theoretically possible in a buggy
        bytecode sequence, for example)
      */
      if(stack.size() < 2){
        throw_HlError("apply: Insufficient number of parameters to variadic function");
      }
      size_t params = stack.size() - 2;
      if(params < 3){
        /*simple and quick dispatch
          for common case
        */
        stack[0] = (*clos)[params];
        /*don't disturb the other
          parameters; the point is
          to be efficient for the
          common case
        */
      } else {
        stack[0] = (*clos)[2]; // f2
        size_t saved_params = params - 2;
        Closure & kclos = *Closure::NewKClosure(proc, saved_params + 3);
        kclos.codereset(proc.global_read(symbols->lookup("$reducto-cont-body")));
        // clos is now invalid
        kclos[0] = stack[0]; // f2
        kclos[1] = stack[1];
        /*** placeholder ***/
        kclos[2] = stack[1];
        for(size_t i = saved_params + 3; i > 3; --i) {
          kclos[i - 1] = stack.top(); stack.pop();
        }
        /*save closure*/
        stack[1] = Object::to_ref(&kclos);
        kclos[2] = Object::to_ref(3);
      }
      goto call_current_closure;
    } NEXT_BYTECODE;
    BYTECODE(reducto_continuation): {
      int N = as_a<int>((*clos)[2]);
      int NN = N + 1; // next N
      stack.push((*clos)[0]);
      if(NN == clos->size()){
        // final iteration
        stack.push((*clos)[1]);
        stack.push(stack[1]);
        stack.push((*clos)[N]);
        /*TODO: insert debug checking for is_a<Generic*> here*/
        attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
        //clos is now invalid
        stack.restack(4);
      } else {
        if(clos->reusable()) {
          // a reusable continuation
          (*clos)[2] = Object::to_ref(NN);
          stack.push(Object::to_ref(clos));
          stack.push(stack[1]);
          stack.push((*clos)[N]);
          stack.restack(4);
        } else {
          /*TODO: instead create a closure
            with only a reference to this
            closure and an index number, to
            reduce memory allocation.
          */
          Closure & nclos = *Closure::NewKClosure(proc, 
                                                  // save only necessary
                                                  clos->size() - NN + 3);
          nclos.codereset(proc.global_read(symbols->lookup("$reducto-cont-body")));
          // clos is now invalid
          SETCLOS(clos); //revalidate clos
          nclos[0] = (*clos)[0];
          nclos[1] = (*clos)[1];
          /*** placeholder! ***/
          nclos[2] = (*clos)[1];
          for(int j = 0; j < clos->size() - NN; ++j){
            nclos[3 + j] = (*clos)[NN + j];
          }
          stack.push(Object::to_ref(&nclos));
          stack.push(stack[1]);
          stack.push((*clos)[N]);
          stack.restack(4);
          // clos is now invalid again
          // nclos is now invalid
          Closure& kkclos = *dynamic_cast<Closure*>(as_a<Generic*>(stack[1]));
          kkclos[2] = Object::to_ref(3);
        }
      }
      goto call_current_closure;
    } NEXT_BYTECODE;
//     BYTECODE(rep): {
//       bytecode_<&Generic::rep>(stack);
//     } NEXT_BYTECODE;
//     BYTECODE(rep_local_push): {
//       INTPARAM(N);
//       bytecode_local_push_<&Generic::rep>(stack, N);
//     } NEXT_BYTECODE;
//     BYTECODE(rep_clos_push): {
//       INTPARAM(N);
//       bytecode_clos_push_<&Generic::rep>(stack, clos, N);
//     } NEXT_BYTECODE;
//     BYTECODE(tag): {
//       bytecode_tag(proc,stack);
//     } NEXT_BYTECODE;
    BYTECODE(sv): {
      bytecode_<&make_sv>(proc, stack);
      SETCLOS(clos); // allocation may invalidate clos
    } NEXT_BYTECODE;
    BYTECODE(sv_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&make_sv>(proc, stack, N);
      SETCLOS(clos); // allocation may invalidate clos
    } NEXT_BYTECODE;
    BYTECODE(sv_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&make_sv>(proc, stack, *clos, N);
      SETCLOS(clos); // allocation may invalidate clos
    } NEXT_BYTECODE;
    BYTECODE(sv_ref): {
      bytecode_<&sv_ref>(stack);
    } NEXT_BYTECODE;
    BYTECODE(sv_ref_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&sv_ref>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_ref_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&sv_ref>(stack, *clos, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_set): {
      bytecode2_<&sv_set>(stack);
    } NEXT_BYTECODE;
    BYTECODE(sym): {
      SYMPARAM(S);
      bytecode_sym(proc, stack, S);
    } NEXT_BYTECODE;
    BYTECODE(symeval): {
      bytecode_symeval(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(table_create): {
      bytecode_table_create(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(table_ref): {
      bytecode_table_ref(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(table_sref): {
      bytecode_table_sref(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(table_map): {
      
    } NEXT_BYTECODE;
//     BYTECODE(type): {
//       bytecode_<&Generic::type>(proc, stack);
//     } NEXT_BYTECODE;
//     BYTECODE(type_local_push): {
//       INTPARAM(N);
//       bytecode_local_push_<&Generic::type>(proc, stack, N);
//     } NEXT_BYTECODE;
//     BYTECODE(type_clos_push): {
//       INTPARAM(N);
//       bytecode_clos_push_<&Generic::type>(proc, stack, clos, N);
//     } NEXT_BYTECODE;
    BYTECODE(variadic): {
      INTPARAM(N);
      bytecode_variadic(proc, stack, N);
      SETCLOS(clos);
    } NEXT_BYTECODE;
    BYTECODE(do_executor): {
      SYMPARAM(s);
      Executor *e = Executor::findExecutor(s);
      if (e) {
        // ?? could this cause problems if an hl function is called
        // ?? by the Executor?
        // !! Not if the Executor sets up the hl stack properly for
        // !! a function call.  In such a case the Executor would
        // !! really be written in CPS, with an Executor before the
        // !! hl-function-call setting up the call to the function,
        // !! creating a continuation structure that will be used
        // !! by the next Executor, which receives the return value
        // !! of the hl function call.
        // !! It does require access to the Process's Heap in order
        // !! to allocate though
        // !! Almost definitely we don't want the Executor to call
        // !! the hl function by calling back into execute().
        if (e->run(proc, reductions))
          goto call_current_closure;
      }
      else {
        std::string err("couldn't find executor: ");
        err += s->getPrintName();
        throw_HlError(err.c_str());
      }
    } NEXT_BYTECODE;
    BYTECODE(plus): {
      bytecode_plus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(minus): {
      bytecode_minus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(mul): {
      bytecode_mul(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(div): {
      bytecode_div(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(mod): {
      bytecode_mod(proc, stack);
    } NEXT_BYTECODE;
  }
}
