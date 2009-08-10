#include"all_defines.hpp"
#include <stdlib.h> // for size_t
#include <string>
#include <sstream>
#include <new> // for std::bad_alloc
#include "types.hpp"
#include "executors.hpp"
#include "bytecodes.hpp"
#include "workers.hpp"
#include "obj_aio.hpp"
#include "assembler.hpp"

#ifdef DEBUG
  #include <typeinfo>
#endif
#include <iostream>

std::map<Symbol*,_bytecode_label> bytetb;
std::map<_bytecode_label, Symbol*> inv_bytetb;


ExecutorTable Executor::tbl;

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

class NON_STD { };

class InitialAssignments {
public:
	InitialAssignments const& operator()(
			char const* s,
			_bytecode_label l,
                        bytecode_arg_type tp = ARG_NONE) const {
                Symbol *opcode = symbols->lookup(s);
		bytetb[opcode] = l;
                inv_bytetb[l] = opcode;
                assembler.regArg(l, tp);
		return *this;
	}
	InitialAssignments const& operator()(
			char const* s,
			_bytecode_label l,
			NON_STD) const {
		(*this)(s, l);
	}
	InitialAssignments const& operator()(
			char const* s,
			_bytecode_label l,
			bytecode_arg_type tp,
			NON_STD) const {
		(*this)(s, l, tp);
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
  if (code.get()==NULL) { // first instruction added
    code.reset(new bytecode_t[64]); // default initial size
    nextCode = 0;
    codeSize = 64;
  } else if (nextCode >= codeSize) {
    bytecode_t *nb = new bytecode_t[codeSize*2];
    bytecode_t *c = code.get();
    for (size_t i = 0; i<codeSize; i++)
      nb[i] = c[i];
    code.reset(nb);
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

/*attempts to deallocate the specified object if it's a reusable
continuation closure
*/
static void attempt_kclos_dealloc(Heap& hp, Generic* gp) {
	Closure* kp = dynamic_cast<Closure*>(gp);
	if(kp == NULL) return;
	if(!kp->reusable()) return;
	hp.lifo_dealloc(gp);
}

#define CLOSUREREF Closure& clos = *known_type<Closure>(stack[0])

#define DOCALL() goto call_current_closure;

ProcessStatus execute(Process& proc, size_t& reductions, Process*& Q, bool init) {

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
      ("<bc>accessor",		THE_BYTECODE_LABEL(accessor))
      ("<bc>accessor-ref",	THE_BYTECODE_LABEL(accessor_ref))
      ("<bc>acquire",		THE_BYTECODE_LABEL(acquire))
      ("<bc>add-event",		THE_BYTECODE_LABEL(add_event))
      ("<bc>apply",		THE_BYTECODE_LABEL(apply), ARG_INT)
      ("<bc>apply-invert-k",	THE_BYTECODE_LABEL(apply_invert_k), ARG_INT)
      ("<bc>apply-k-release",	THE_BYTECODE_LABEL(apply_k_release), ARG_INT)
      ("<bc>apply-list",	THE_BYTECODE_LABEL(apply_list), ARG_INT)
      ("<bc>arg-dispatch",	THE_BYTECODE_LABEL(arg_dispatch), ARG_INT)
      ("<bc>b-ref",		THE_BYTECODE_LABEL(b_ref))
      ("<bc>b-len",		THE_BYTECODE_LABEL(b_len))
      ("<bc>bounded",		THE_BYTECODE_LABEL(bounded))
      /*these implement the actual bytecodes <bc>closure, <bc>k-closure,
      <bc>k-closure-recreate, and <bc>k-closure-reuse.
      */
      ("<bc>build-closure", THE_BYTECODE_LABEL(build_closure), ARG_INT, 
         NON_STD())
      ("<bc>build-k-closure", THE_BYTECODE_LABEL(build_k_closure), ARG_INT,
         NON_STD() )
      ("<bc>build-k-closure-recreate",THE_BYTECODE_LABEL(build_k_closure_recreate),
       ARG_INT,
         NON_STD() )
      ("<bc>build-k-closure-reuse", THE_BYTECODE_LABEL(build_k_closure_reuse), 
       ARG_INT,
         NON_STD() )
      ("<bc>car",			THE_BYTECODE_LABEL(car))
      ("<bc>scar",                  THE_BYTECODE_LABEL(scar))
      ("<bc>car-local-push",	THE_BYTECODE_LABEL(car_local_push), ARG_INT)
      ("<bc>car-clos-push",	THE_BYTECODE_LABEL(car_clos_push), ARG_INT)
      ("<bc>cdr",			THE_BYTECODE_LABEL(cdr))
      ("<bc>scdr",                  THE_BYTECODE_LABEL(scdr))
      ("<bc>cdr-local-push",	THE_BYTECODE_LABEL(cdr_local_push), ARG_INT)
      ("<bc>cdr-clos-push",	THE_BYTECODE_LABEL(cdr_clos_push), ARG_INT)
      ("<bc>char",			THE_BYTECODE_LABEL(b_char), ARG_INT)
      ("<bc>check-vars",	       THE_BYTECODE_LABEL(check_vars), ARG_INT)
      ("<bc>closure-ref",	      THE_BYTECODE_LABEL(closure_ref), ARG_INT)
      ("<bc>composeo",		THE_BYTECODE_LABEL(composeo))
      ("<bc>cons",		THE_BYTECODE_LABEL(cons))
      ("<bc>const-ref",     THE_BYTECODE_LABEL(const_ref), ARG_INT)
      ("<bc>continue",		THE_BYTECODE_LABEL(b_continue))
      ("<bc>continue-local",	THE_BYTECODE_LABEL(continue_local), ARG_INT)
      ("<bc>continue-on-clos",	THE_BYTECODE_LABEL(continue_on_clos), ARG_INT)
      ("<bc>debug-backtrace", THE_BYTECODE_LABEL(debug_backtrace))
      ("<bc>debug-bytecode-info", THE_BYTECODE_LABEL(debug_bytecode_info))
      ("<bc>debug-call", THE_BYTECODE_LABEL(debug_call), ARG_INT)
      ("<bc>debug-tail-call", THE_BYTECODE_LABEL(debug_tail_call), ARG_INT)
      /*this implements <common>disclose*/
      // !! Could also be done as an Executor
      ("<bc>disclose", THE_BYTECODE_LABEL(disclose), NON_STD() )
      ("<bc>empty-event-set",	THE_BYTECODE_LABEL(empty_event_set))
      ("<bc>enclose", THE_BYTECODE_LABEL(enclose), NON_STD())
      ("<bc>err-handler",	THE_BYTECODE_LABEL(err_handler))
      ("<bc>err-handler-set",	THE_BYTECODE_LABEL(err_handler_set))
      ("<bc>event-poll",	THE_BYTECODE_LABEL(event_poll))
      ("<bc>event-wait",	THE_BYTECODE_LABEL(event_wait))
      ("<bc>f-to-i",		THE_BYTECODE_LABEL(f_to_i))
      ("<bc>get-history", THE_BYTECODE_LABEL(get_history))
      ("<bc>global",		THE_BYTECODE_LABEL(global), ARG_SYMBOL)
      ("<bc>global-set",		THE_BYTECODE_LABEL(global_set), ARG_SYMBOL)
      ("<bc>halt",		THE_BYTECODE_LABEL(halt))
      ("<bc>halt-local-push",	THE_BYTECODE_LABEL(halt_local_push), ARG_INT)
      ("<bc>halt-clos-push",	THE_BYTECODE_LABEL(halt_clos_push), ARG_INT)
      ("<bc>i-to-f",		THE_BYTECODE_LABEL(i_to_f))
      ("<bc>is",		THE_BYTECODE_LABEL(is))
      ("<bc>jmp-nil",		THE_BYTECODE_LABEL(jmp_nil), ARG_INT,
         NON_STD() ) // replacement for 'if
      ("<bc>int",			THE_BYTECODE_LABEL(b_int), ARG_INT)
      ("<bc>i/o-accept",	THE_BYTECODE_LABEL(io_accept))
      ("<bc>i/o-appendfile",	THE_BYTECODE_LABEL(io_appendfile))
      ("<bc>i/o-close",		THE_BYTECODE_LABEL(io_close))
      ("<bc>i/o-connect",	THE_BYTECODE_LABEL(io_connect))
      ("<bc>i/o-fsync",		THE_BYTECODE_LABEL(io_fsync))
      ("<bc>i/o-infile",	THE_BYTECODE_LABEL(io_infile))
      ("<bc>i/o-listener",	THE_BYTECODE_LABEL(io_listener))
      ("<bc>i/o-outfile",	THE_BYTECODE_LABEL(io_outfile))
      ("<bc>i/o-pipe",		THE_BYTECODE_LABEL(io_pipe))
      ("<bc>i/o-read",		THE_BYTECODE_LABEL(io_read))
      ("<bc>i/o-seek",		THE_BYTECODE_LABEL(io_seek))
      ("<bc>i/o-stderr",	THE_BYTECODE_LABEL(io_stderr))
      ("<bc>i/o-stdin",		THE_BYTECODE_LABEL(io_stdin))
      ("<bc>i/o-stdout",	THE_BYTECODE_LABEL(io_stdout))
      ("<bc>i/o-tell",		THE_BYTECODE_LABEL(io_tell))
      ("<bc>i/o-write",		THE_BYTECODE_LABEL(io_write))
      ("<bc>ccc", THE_BYTECODE_LABEL(ccc))
      ("<bc>l-to-b",		THE_BYTECODE_LABEL(l_to_b))
      ("<bc>lit-nil",		THE_BYTECODE_LABEL(lit_nil))
      ("<bc>lit-t",		THE_BYTECODE_LABEL(lit_t))
      ("<bc>local",		THE_BYTECODE_LABEL(local), ARG_INT)
      ("<bc>monomethod",		THE_BYTECODE_LABEL(monomethod))
      ("<bc>only-running",	THE_BYTECODE_LABEL(only_running))
      ("<bc>proc-local",	THE_BYTECODE_LABEL(proc_local))
      ("<bc>proc-local-set",	THE_BYTECODE_LABEL(proc_local_set))
      ("<bc>recv", THE_BYTECODE_LABEL(recv))
      ("<bc>reducto",		THE_BYTECODE_LABEL(reducto))
      ("<bc>reducto-continuation",   THE_BYTECODE_LABEL(reducto_continuation))
      ("<bc>release",		THE_BYTECODE_LABEL(release))
      ("<bc>remove-event",	THE_BYTECODE_LABEL(remove_event))
      ("<bc>rep",		THE_BYTECODE_LABEL(rep))
      ("<bc>rep-local-push",	THE_BYTECODE_LABEL(rep_local_push))
      ("<bc>rep-clos-push",	THE_BYTECODE_LABEL(rep_clos_push))
      ("<bc>s-to-sy",		THE_BYTECODE_LABEL(s_to_sy))
      ("<bc>self-pid", THE_BYTECODE_LABEL(self_pid))
      ("<bc>send",		THE_BYTECODE_LABEL(send))
      ("<bc>sleep",		THE_BYTECODE_LABEL(sleep))
      ("<bc>spawn",		THE_BYTECODE_LABEL(spawn))
      ("<bc>string-build",	THE_BYTECODE_LABEL(string_build))
      ("<bc>string-create",	THE_BYTECODE_LABEL(string_create), ARG_INT)
      ("<bc>string-length",		THE_BYTECODE_LABEL(string_length))
      ("<bc>string-ref",		THE_BYTECODE_LABEL(string_ref))
      ("<bc>string-sref",		THE_BYTECODE_LABEL(string_sref))
      ("<bc>sv",			THE_BYTECODE_LABEL(sv))
      ("<bc>sv-local-push",	THE_BYTECODE_LABEL(sv_local_push), ARG_INT)
      ("<bc>sv-clos-push",	THE_BYTECODE_LABEL(sv_clos_push), ARG_INT)
      ("<bc>sv-ref",		THE_BYTECODE_LABEL(sv_ref))
      ("<bc>sv-ref-local-push",    THE_BYTECODE_LABEL(sv_ref_local_push), ARG_INT)
      ("<bc>sv-ref-clos-push",	THE_BYTECODE_LABEL(sv_ref_clos_push), ARG_INT)
      ("<bc>sv-set",		THE_BYTECODE_LABEL(sv_set))
      ("<bc>sy-to-s",		THE_BYTECODE_LABEL(sy_to_s))
      ("<bc>sym",			THE_BYTECODE_LABEL(sym), ARG_SYMBOL)
      ("<bc>symeval",		THE_BYTECODE_LABEL(symeval), ARG_SYMBOL)
      ("<bc>system",		THE_BYTECODE_LABEL(system))
      ("<bc>table-create",		THE_BYTECODE_LABEL(table_create))
      ("<bc>table-ref",		THE_BYTECODE_LABEL(table_ref))
      ("<bc>table-sref",		THE_BYTECODE_LABEL(table_sref))
      ("<bc>table-keys",		THE_BYTECODE_LABEL(table_keys))
      ("<bc>tag",			THE_BYTECODE_LABEL(tag))
      ("<bc>try-recv", THE_BYTECODE_LABEL(try_recv))
      ("<bc>type",		THE_BYTECODE_LABEL(type))
      ("<bc>type-local-push",	THE_BYTECODE_LABEL(type_local_push))
      ("<bc>type-clos-push",	THE_BYTECODE_LABEL(type_clos_push))
      ("<bc>variadic",		THE_BYTECODE_LABEL(variadic), ARG_INT)
      ("<bc>do-executor",       THE_BYTECODE_LABEL(do_executor), ARG_SYMBOL,
        NON_STD())
      ("<bc>i+",                    THE_BYTECODE_LABEL(iplus))
      ("<bc>i-",                    THE_BYTECODE_LABEL(iminus))
      ("<bc>i*",                    THE_BYTECODE_LABEL(imul))
      ("<bc>i/",                    THE_BYTECODE_LABEL(idiv))
      ("<bc>imod",			THE_BYTECODE_LABEL(imod))
      ("<bc>i<",                    THE_BYTECODE_LABEL(iless))
      ("<bc>f+",                    THE_BYTECODE_LABEL(fplus))
      ("<bc>f-",                    THE_BYTECODE_LABEL(fminus))
      ("<bc>f*",                    THE_BYTECODE_LABEL(fmul))
      ("<bc>f/",                    THE_BYTECODE_LABEL(fdiv))
      ("<bc>f<",                    THE_BYTECODE_LABEL(fless))
      /*declare executors*/
      ("<impl>is-symbol-packaged",	THE_EXECUTOR<IsSymbolPackaged>())
      ("<impl>assemble",		THE_EXECUTOR<AssemblerExecutor>())
      /*assign bultin global*/
      ;/*end initializer*/

    // initialize assembler operations
    assembler.reg<ClosureAs>(symbols->lookup("<bc>closure"),
                             THE_BYTECODE_LABEL(const_ref));
    assembler.reg<KClosureAs>(symbols->lookup("<bc>k-closure"), NULL_BYTECODE);
    assembler.reg<KClosureRecreateAs>(symbols->lookup("<bc>k-closure-recreate"),
                                      NULL_BYTECODE);
    assembler.reg<KClosureReuseAs>(symbols->lookup("<bc>k-closure-reuse"), 
                                      NULL_BYTECODE);
    assembler.reg<IfAs>(symbols->lookup("<bc>if"), 
                              THE_BYTECODE_LABEL(jmp_nil));
    assembler.reg<ComplexAs<Float> >(symbols->lookup("<bc>float"), NULL_BYTECODE);
    assembler.reg<DbgInfoAs<&Bytecode::set_name> >(symbols->lookup("<bc>debug-name"), NULL_BYTECODE);
    assembler.reg<DbgInfoAs<&Bytecode::set_line> >(symbols->lookup("<bc>debug-line"), NULL_BYTECODE);
    assembler.reg<DbgInfoAs<&Bytecode::set_file> >(symbols->lookup("<bc>debug-file"), NULL_BYTECODE);

    /*
     * build and assemble various bytecode sequences
     * these will hold a fixed Bytecodes that will be used
     * as the continuations for various bytecodes
     */
    symbols->lookup("<impl>reducto-cont-body")->
      set_value(Assembler::inline_assemble(proc, 
        "(<bc>reducto-continuation) (<bc>continue)"));
    symbols->lookup("<impl>ccc-fn-body")->
      set_value(Assembler::inline_assemble(proc, "(<bc>check-vars 3) (<bc>continue-on-clos 0)"));
    symbols->lookup("<impl>composeo-cont-body")->
      set_value(Assembler::inline_assemble(proc,
        "(<bc>check-vars 2) "
        "(<bc>closure-ref 0) "
        "(<bc>closure-ref 1) "
        "(<bc>local 1) "
        "(<bc>apply-k-release 3) "
        )
      );

    return process_dead;
  }
  // main VM loop
  // add bytecode as an extra root object to scan
  Object::ref& bytecode = proc.bytecode_slot;
  // ?? could this approach be used for clos too?
 call_current_closure:
  if(--reductions == 0) {
    return process_running;
  }
  // get current closure
#ifdef DEBUG
  if (is_a<int>(stack[0]))
    std::cerr << "Type on stacktop: int" << std::endl;
  else {
    std::type_info const &inf = typeid(as_a<Generic*>(stack[0]));
    std::cerr << "Type on stacktop: " << inf.name() << std::endl;
  }
#endif
  // is this a function/continuation call?
  {Closure *clos = maybe_type<Closure>(stack[0]);
    if (!clos) {
      /*
      In hl, function calls of the form (f a b c),
      where 'f is *not* a real function, are
      transformed into function calls of
      (<hl>call* f a b c)
      - if <hl>call* is itself not a function, the
        behavior is undefined.
      This allows the user to redefine how an
      object behaves when it is called by simply
        defining a method for <hl>call*.
      */
      /*Transform stack:
      from:
         obj   k   a1   a2   ...
      to:
         call* k   obj  a1   a2    ...
      */
      stack.push(Object::nil());
      Object::ref call_star = proc.global_read(symbol_call_star);
      if(!maybe_type<Closure>(call_star)) {
        throw_HlError("<hl>call* is not a function");
      }
      /*move args*/
      for(size_t i = 1; i < stack.size() - 1; ++i) {
        stack.top(i) = stack.top(i + 1);
      }
      stack[2] = stack[0];
      stack[0] = call_star;
      /***/ DOCALL(); /***/
    }
    // a reference to the current bytecode *must* be retained for 
    // k-closure-recreate and k-closure-reuse to work correctly:
    // they may change the body of the current closure, but we are still
    // executing the old body and if we don't retain it we can't access it from
    // clos->code(), since clos->code() may now refer to a different body
    bytecode = clos->code();
  }
  // to start, call the closure in stack[0]
  DISPATCH_BYTECODES {
    BYTECODE(accessor): {
      /*check types*/
      Closure* orig =
      expect_type<Closure>(stack.top(2),
        "'accessor expects two functions, first param is not a function"
      );
      expect_type<Closure>(stack.top(1),
        "'accessor expects two functions, second param is not a function"
      );
      /*fixups*/
      size_t sz = orig->size();
      /*if original already had a write accessor, overwrite it
      in the return value
      */
      if(sz > 0 && maybe_type<AccessorSlot>((*orig)[sz - 1])) {
        --sz;
      }
      /*alloc*/
      stack.push(
        Object::to_ref<Generic*>(proc.create_variadic<Closure>(sz + 1))
      );
      // top(3) = prog, top(2) = writer, top(1) = return value
      AccessorSlot* slot = proc.create<AccessorSlot>();
      /*re-read*/
      orig = known_type<Closure>(stack.top(3));
      Closure* rv = known_type<Closure>(stack.top(1));
      for(size_t i = 0; i < sz; ++i) {
        (*rv)[i] = (*orig)[i];
      }
      rv->codereset(orig->code());
      /*load slot*/
      (*rv)[sz] = Object::to_ref<Generic*>(slot);
      slot->slot = stack.top(2);
      /*manipulate stack*/
      stack.top(3) = stack.top(1);
      stack.pop(2);
    } NEXT_BYTECODE;
    BYTECODE(accessor_ref): {
      Closure* v = expect_type<Closure>( stack.top(),
        "'accessor-ref expects a function"
      );
      size_t sz = v->size();
      if(sz == 0) {
        stack.top() = Object::nil();
      } else {
        AccessorSlot* slot = maybe_type<AccessorSlot>((*v)[sz - 1]);
        if(slot) {
          stack.top() = slot->slot;
        } else {
          stack.top() = Object::nil();
        }
      }
    } NEXT_BYTECODE;
    BYTECODE(acquire): {
      proc.global_acquire();
    } NEXT_BYTECODE;
    BYTECODE(add_event): {
      bytecode_<&add_event>(stack);
    } NEXT_BYTECODE;
    BYTECODE(apply): {
      INTPARAM(N);
      stack.restack(N);
      /***/ DOCALL(); /***/
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(apply_list): {
      Object::ref tmp;
      stack.restack(3);
      /*destructure until stack top is nil*/
      while(stack.top()!=Object::nil()){
        tmp = stack.top();
        bytecode_<&car>(stack);
        // we don't expect car to
        // allocate, so tmp should
        // still be valid
        stack.push(tmp);
        bytecode_<&cdr>(stack);
      }
      stack.pop();
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(arg_dispatch): {
      /* implements dispatching based on
       * number of hl-side arguments.
       * Given 0 to (N-2) arguments,
       * passes to the corresponding
       * function in the current
       * closure.  For (N-1) or more
       * arguments, passes to the last
       * function in the current
       * closure.
       */
      INTPARAM(N); CLOSUREREF;
      size_t as = stack.size() - 2;
      if(as > N - 1) as = N - 1;
      stack[0] = clos[as];
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(b_ref): {
      bytecode2_<&BinObj::bin_ref>( stack );
    } NEXT_BYTECODE;
    BYTECODE(b_len): {
      bytecode_<&BinObj::bin_len>( stack );
    } NEXT_BYTECODE;
    BYTECODE(bounded): {
      bytecode_bounded(stack);
    } NEXT_BYTECODE;
    BYTECODE(build_closure): {
      INTPARAM(N);
      Closure* nclos = Closure::NewClosure(proc, N);
      nclos->codereset(stack.top()); stack.pop();
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
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(build_k_closure_recreate): {
      /*TODO: insert debug checking for is_a<Generic*> here*/
      //      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      /*put a random object in stack[0]*/
      //stack[0] = stack[1];
      // !! SETCLOS will fail!!
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
        CLOSUREREF;
        nclos = Closure::NewKClosure(proc, clos.size());
        //clos is now invalid
        nclos->codereset(stack.top()); stack.pop();
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
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<&car>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr): {
      bytecode_<&cdr>(stack);
    } NEXT_BYTECODE;
    BYTECODE(scdr): {
      bytecode2_<&scdr>(stack);
    } NEXT_BYTECODE;
    BYTECODE(cdr_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&cdr>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr_clos_push): {
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<&cdr>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(b_char): {
      INTPARAM(N);
      bytecode_char(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(check_vars): {
      INTPARAM(N);
      bytecode_check_vars(stack, N);
      // register function args
      // skip closure & continuation
			proc.history.register_args(stack, 2, N);
    } NEXT_BYTECODE;
    BYTECODE(closure_ref): {
      INTPARAM(N); CLOSUREREF;
      bytecode_closure_ref(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(composeo): {
      CLOSUREREF;
      /*destructure closure*/
      stack.push(clos[0]);
      stack[0] = clos[1];
      // create continuation
      Closure& kclos = *Closure::NewKClosure(proc, 2); 
      // clos is now invalid
      // !! should really avoid SymbolsTable::lookup() due to
      // !! increased lock contention
      kclos.codereset(proc.global_read(symbols->lookup("<impl>composeo-cont-body")));
      /*next function*/
      kclos[0] = stack.top(); stack.pop();
      /*continuation*/
      kclos[1] = stack[1];
      stack[1] = Object::to_ref(&kclos);
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(cons): {
      bytecode_cons(proc,stack);
    } NEXT_BYTECODE;
    BYTECODE(const_ref): {
      INTPARAM(i);
      // !! too many indirections and conversions
      // !! consider holding a reference to the current Bytecode*
      Bytecode &b = *known_type<Bytecode>(bytecode);
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
      proc.history.leave();
      /***/ DOCALL(); /***/
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
      proc.history.leave();
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    /*
      handles the case where the
      continuation we want to return
      to is in our own closure.
    */
    BYTECODE(continue_on_clos): {
      INTPARAM(N); CLOSUREREF;
      Object::ref gp = stack.top();
      stack.top() = clos[N];
      stack.push(gp);
      /*TODO: insert debug checking for is_a<Generic*> here*/
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      stack.restack(2);
      proc.history.leave();
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    // take a bytecode object from the stack
    // leave a list holding (name file line) of the bytecode
    BYTECODE(debug_bytecode_info): {
      Bytecode *b = expect_type<Bytecode>(stack.top()); stack.pop();
      b->info(proc);
    } NEXT_BYTECODE;
    // register a function call
    // expect closure to register in stack.top(N)
    BYTECODE(debug_call): {
      INTPARAM(N);
      proc.history.enter(stack.top(N));
    } NEXT_BYTECODE;
    // similar ro debug_call, but register a tail call
    BYTECODE(debug_tail_call): {
      INTPARAM(N);
      proc.history.enter_tail(stack.top(N));
    } NEXT_BYTECODE;
    // leave a list of called closures on the stack
    BYTECODE(debug_backtrace): {
      proc.history.to_list(proc);
    } NEXT_BYTECODE;
    // take a closure, leave a list on the stack with
    // the bytecode object and the enclosed vars
    BYTECODE(disclose): {
       Closure *c = expect_type<Closure>(stack.top());
       stack.pop();
       stack.push(c->code());
       int sz = c->size();
       /*skip the last entry if this Closure has an
       accessor slot
       */
       if(sz > 0 && maybe_type<AccessorSlot>((*c)[sz - 1])) {
         --sz;
       }
       for (int i = 0; i < sz; ++i) {
         stack.push((*c)[i]);
       }
       // list's end
       stack.push(Object::nil());
       // build the list
       sz += 1; // for the bytecode object
       for (int i = 0; i < sz; ++i) {
         bytecode_cons(proc, proc.stack);
       }
    } NEXT_BYTECODE;
    BYTECODE(empty_event_set): {
      bytecode_<&empty_event_set>(stack);
    } NEXT_BYTECODE;
    // take a bytecode object and a list of values to enclose
    BYTECODE(enclose): {
      Object::ref vals = stack.top(); stack.pop();
      // unrolls vals in the stack
      size_t n = 0; // count number of values
      for (Object::ref lst = vals; maybe_type<Cons>(lst); lst = cdr(lst)) {
        stack.push(car(lst));
        n++;
      }
      Closure* nclos = Closure::NewClosure(proc, n);
      // enclose values
      for(int i = n; i; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      // get the bytecode
      Object::ref body = stack.top(); stack.pop();
      // body must be a bytecode
      check_type<Bytecode>(body);
      nclos->codereset(body);
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(err_handler): {
      bytecode_proc_get<&Process::err_handler_slot>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(err_handler_set): {
      bytecode_proc_set<&Process::err_handler_slot>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(event_poll): {
      bytecode_<&event_poll>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(event_wait): {
      bytecode_<&event_wait>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(f_to_i): {
      bytecode_<&f_to_i>(stack);
    } NEXT_BYTECODE;
    // get current history
    BYTECODE(get_history): {
      proc.history.to_list(proc);
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
      INTPARAM(N); CLOSUREREF;
      stack.push(clos[N]);
      stack.restack(1);
      return process_dead;
    } NEXT_BYTECODE;
    BYTECODE(i_to_f): {
      bytecode_<&i_to_f>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_accept): {
      bytecode2_<&io_accept>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_appendfile): {
      bytecode2_<&io_openfile<&appendfile> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_close): {
      bytecode_<&io_close >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_connect): {
      bytecode3_<&io_connect >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_fsync): {
      bytecode2_<&io_fsync >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_infile): {
      bytecode2_<&io_openfile<&infile> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_listener): {
      bytecode2_<&io_listener >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_outfile): {
      bytecode2_<&io_openfile<&outfile> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_pipe): {
      bytecode_io_pipe(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_read): {
      bytecode3_<&io_read >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_seek): {
      bytecode2_<&io_seek >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_stderr): {
      bytecode_<&io_builtin_port<&port_stderr> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_stdin): {
      bytecode_<&io_builtin_port<&port_stdin> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_stdout): {
      bytecode_<&io_builtin_port<&port_stdout> >(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(io_tell): {
      bytecode_<&io_tell>(proc,stack);
    } NEXT_BYTECODE;
    BYTECODE(io_write): {
      bytecode3_<&io_write>(proc,stack);
    } NEXT_BYTECODE;
    BYTECODE(is): {
      bytecode2_<&obj_is>(stack);
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
    BYTECODE(ccc): {
      // expect current continuation and function on the stack
      Closure *k = expect_type<Closure>(stack[1], "ccc expects a continuation");
      Closure *f = expect_type<Closure>(stack[2], "ccc expects a closure");
      k->banreuse(); // continuation can't be reused
      // now call f
      stack[0] = Object::to_ref(f);
      // stack[1] already holds current continuation
      Closure *arg = Closure::NewClosure(proc, 1);
      // !! should really avoid SymbolsTable::lookup() due to
      // !! increased lock contention
      arg->codereset(proc.global_read(symbols->lookup("<impl>ccc-fn-body")));
      (*arg)[0] = Object::to_ref(k); // close other current continuation
      stack[2] = Object::to_ref(arg);
      //(f current-continuation function-that-will-call-current-continuation)
      // do the call
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(l_to_b): {
      bytecode_<&BinObj::from_Cons>(proc, stack);
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
      CLOSUREREF;
      HlTable& T = *known_type<HlTable>(clos[0]);
      if(stack.size() >= 3) {
         Object::ref tp = type(stack[2]);
         Object::ref f = T.lookup(tp);
         if(f) stack[0] = f;
         else  stack[0] = T.lookup(Object::nil());
      } else {
         stack[0] = T.lookup(Object::nil());
      }
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(only_running): {
      bytecode_<&only_running>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(proc_local): {
      bytecode_proc_get<&Process::proc_local_slot>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(proc_local_set): {
      bytecode_proc_set<&Process::proc_local_slot>(proc, stack);
    } NEXT_BYTECODE;
    // call current continuation
    BYTECODE(recv): {
      Object::ref msg;
      if (proc.mailbox().extract_message(msg)) {
        //std::cerr<<"recv: "<<msg<<"\n";
        stack.push(stack[1]); // current continuation
        stack.push(msg);
        stack.restack(2);
        proc.maybe_clear_other_spaces();
        DOCALL();
      } else {
        //std::cerr<<"recv: queue empty\n";
        // <bc>recv is always called in tail position
        // !! NOTE!  If Process::extract_message() above ever
        // !! returns false, we should not change anything in the
        // !! process!
        // !! IN PARTICULAR: we cannot leave any Process::ExtraRoot
        // !! objects hanging around.
        return process_waiting;
      }
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
      TODO: rreducto, which is like reducto
      except with right-to-left reduction.
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
        CLOSUREREF;
        /*simple and quick dispatch
          for common case
        */
        stack[0] = clos[params];
        /*don't disturb the other
          parameters; the point is
          to be efficient for the
          common case
        */
      } else {
        CLOSUREREF;
        size_t saved_params = params - 2;
        stack[0] = clos[2]; // f2
        // !! should really avoid SymbolsTable::lookup() due to
        // !! increased lock contention
        Closure & kclos = *Closure::NewKClosure(proc, saved_params + 3);
        // clos is now invalid
        kclos.codereset(proc.global_read(symbols->lookup("<impl>reducto-cont-body")));
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(reducto_continuation): {
      CLOSUREREF;
      int N = as_a<int>(clos[2]);
      int NN = N + 1; // next N
      stack.push(clos[0]);
      if(NN == clos.size()){
        // final iteration
        stack.push(clos[1]);
        stack.push(stack[1]);
        stack.push(clos[N]);
        /*TODO: insert debug checking for is_a<Generic*> here*/
        attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
        //clos is now invalid
        stack.restack(4);
      } else {
        if(clos.reusable()) {
          // a reusable continuation
          clos[2] = Object::to_ref(NN);
          stack.push(Object::to_ref(&clos));
          stack.push(stack[1]);
          stack.push(clos[N]);
          stack.restack(4);
        } else {
          /*TODO: instead create a closure
            with only a reference to this
            closure and an index number, to
            reduce memory allocation.
          */
          Closure & nclos = *Closure::NewKClosure(proc,
                                                  // save only necessary
                                                  clos.size() - NN + 3);
          // !! should really avoid SymbolsTable::lookup() due to
          // !! increased lock contention
          nclos.codereset(proc.global_read(symbols->lookup("<impl>reducto-cont-body")));
          // clos is now invalid
          {CLOSUREREF; //revalidate
            nclos[0] = clos[0];
            nclos[1] = clos[1];
            /*** placeholder! ***/
            nclos[2] = clos[1];
            for(int j = 0; j < clos.size() - NN; ++j){
              nclos[3 + j] = clos[NN + j];
            }
            stack.push(Object::to_ref(&nclos));
            stack.push(stack[1]);
            stack.push(clos[N]);
            stack.restack(4);
          } // clos is now invalid again
          // nclos is now invalid
          Closure& kkclos = *dynamic_cast<Closure*>(as_a<Generic*>(stack[1]));
          kkclos[2] = Object::to_ref(3);
        }
      }
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(release): {
      /*On this machine, <bc>release does nothing*/
    } NEXT_BYTECODE;
    BYTECODE(remove_event): {
      bytecode_<&remove_event>(stack);
    } NEXT_BYTECODE;
    BYTECODE(rep): {
      bytecode_<rep>(stack);
    } NEXT_BYTECODE;
    BYTECODE(rep_local_push): {
      INTPARAM(N);
      bytecode_local_push_<rep>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(rep_clos_push): {
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<rep>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(s_to_sy): {
      bytecode_s_to_sy(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(tag): {
      bytecode_tag(proc,stack);
    } NEXT_BYTECODE;
    // build an HlPid of the running process
    BYTECODE(self_pid): {
      HlPid *pid = proc.create<HlPid>();
      pid->process = &proc;
      stack.push(Object::to_ref(pid));
    } NEXT_BYTECODE;
    // expect a pid and a message on the stack
    // must be called in tail position
    BYTECODE(send): {
      Object::ref msg = proc.stack.top();
      HlPid *pid = expect_type<HlPid>(proc.stack.top(2), "send expects a pid as first argument");
      ValueHolderRef ref;
      ValueHolder::copy_object(ref, msg);
      bool is_waiting = false;
      if (!pid->process->mailbox().receive_message(ref, is_waiting)) {
        // TODO: save instruction counter in order to restart
        // the process from the correct position
        // !! maybe not necessary: we can always specify that
        // !! <axiom>send may cause the function it is in to
        // !! be arbitrarily restarted, and require that this
        // !! axiom be protected by keeping it in its own
        // !! dedicated function which can safely be restarted
        // !! in case message sending is unsuccessful.
        // !! Or alternatively, just specify that message
        // !! sending/receiving is done at the common layer
        // !! and the axiomatic stuff behind it is
        // !! implementation-specific.
        return process_running;
      } else {
        // prepare the stack for the next call
        stack.push(stack[1]); // push current continuation
        stack.push(msg); // pass a meaningful value to continuation
        stack.restack(2);
        // was process waiting?
        if (is_waiting) {
          // let the process run
          Q = pid->process;
          return process_change;
        } else {
          /***/ DOCALL(); /***/
        }
      }
    } NEXT_BYTECODE;
    BYTECODE(sleep): {
      bytecode2_<&create_sleep_event>(proc, stack);
    } NEXT_BYTECODE;
    // call current continuation, passing the pid of created process
    // !! WARNING: <bc>spawn MUST be called WITHIN a closure NOT a
    // !! continuation, since it expects current continuation in
    // !! stack[1], not in stack[0]
    BYTECODE(spawn): {
      AllWorkers &w = AllWorkers::getInstance();
      // create new process 
      HlPid *spawned = proc.spawn(stack.top()); stack.pop();
      // release cpu as soon as possible
      // spawn is required to appear in tail position.
      stack.push(stack[1]); // current cont.
      stack.push(Object::to_ref(spawned)); // the pid
      stack.restack(2);
      // process is ready to run, register it to working queue
      w.register_process(spawned->process);
      // w.workqueue_push(spawned->process);
      Q = spawned->process; // next to run
      return process_change;
    } NEXT_BYTECODE;
    BYTECODE(string_build): {
      int N = as_a<int>(stack.top()); stack.pop();
      bytecode_string_build( proc, stack, N );
    } NEXT_BYTECODE;
    BYTECODE(string_create): {
      INTPARAM(N); // length of string to create from stack
      bytecode_string_create( proc, stack, N );
    } NEXT_BYTECODE;
    BYTECODE(string_length): {
      bytecode_<&HlString::length>( stack );
    } NEXT_BYTECODE;
    BYTECODE(string_ref): {
      bytecode2_<&HlString::string_ref>( stack );
    } NEXT_BYTECODE;
    BYTECODE(string_sref): {
      bytecode_string_sref( proc, stack );
      // sref *can* allocate, if string is used as key in table...
    } NEXT_BYTECODE;
    BYTECODE(sv): {
      bytecode_<&make_sv>(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(sv_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&make_sv>(proc, stack, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_clos_push): {
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<&make_sv>(proc, stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_ref): {
      bytecode_<&sv_ref>(stack);
    } NEXT_BYTECODE;
    BYTECODE(sv_ref_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&sv_ref>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_ref_clos_push): {
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<&sv_ref>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(sv_set): {
      bytecode2_<&sv_set>(stack);
    } NEXT_BYTECODE;
    BYTECODE(sy_to_s): {
      bytecode_sy_to_s(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(sym): {
      SYMPARAM(S);
      bytecode_sym(proc, stack, S);
    } NEXT_BYTECODE;
    BYTECODE(symeval): {
      bytecode_symeval(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(system): {
      bytecode2_<&create_system_event>(proc, stack);
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
    BYTECODE(table_keys): {
      bytecode_table_keys(proc, stack);
    } NEXT_BYTECODE;
    // expects two functions on the stack
    // one is called if there is a message, the other if 
    // the message queue is empty
    // stack is:
    // -- top --
    // fail function
    // success function
    // -- bottom --
    BYTECODE(try_recv): {
      MailBox mbox = proc.mailbox();
      Object::ref msg;
      bool has_message;
      bool tried = mbox.try_extract_message(msg, has_message);
      if(!tried) {
        return process_running;
      } else if (has_message) {
        // success
        stack.push(stack.top(2));
        stack.push(stack[1]);
        stack.push(msg);
        stack.restack(3);
        proc.maybe_clear_other_spaces();
      } else {
        // fail
        stack.push(stack.top(1));
        stack.push(stack[1]);
        stack.restack(2);
      }
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    /*type never allocates: even when
    using a builtin type, we just use
    a Object::to_ref<Symbol*>()
    */
    BYTECODE(type): {
      bytecode_<&type>(stack);
    } NEXT_BYTECODE;
    BYTECODE(type_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&type>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(type_clos_push): {
      INTPARAM(N); CLOSUREREF;
      bytecode_clos_push_<&type>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(variadic): {
      INTPARAM(N);
      bytecode_variadic(proc, stack, N);
      // register args
      // N+1 to register variadic arg
			proc.history.register_args(stack, 2, N+1);
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
          DOCALL();
      }
      else {
        std::string err("couldn't find executor: ");
        err += s->getPrintName();
        throw_HlError(err.c_str());
      }
    } NEXT_BYTECODE;
    BYTECODE(iplus): {
      /*currently, integer maths don't actually
      need access to the Process, but we may
      want to implement bigint's in the future.
      */
      bytecode_iplus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(iminus): {
      bytecode_iminus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(imul): {
      bytecode_imul(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(idiv): {
      bytecode_idiv(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(imod): {
      bytecode_imod(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(iless): {
      bytecode_iless(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(fplus): {
      bytecode_fplus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(fminus): {
      bytecode_fminus(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(fmul): {
      bytecode_fmul(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(fdiv): {
      bytecode_fdiv(proc, stack);
    } NEXT_BYTECODE;
    BYTECODE(fless): {
      bytecode_fless(proc, stack);
    } NEXT_BYTECODE;
  }
  // execution shouldn't reach this point
  throw_HlError("internal: end of execute() reached");
}
