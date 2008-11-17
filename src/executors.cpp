#include"all_defines.hpp"
#include <stdlib.h> // for size_t
#include <string>
#include "types.hpp"
#include "executors.hpp"
#include "bytecodes.hpp"

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

class InitialAssignments {
public:
	InitialAssignments const& operator()(
			char const* s,
			_bytecode_label l) const{
		bytetb[symbols->lookup(s)] = l;
		return *this;
	}
};

void assemble(BytecodeSeq & seq, bytecode_t* & a_seq) {
  a_seq = new bytecode_t[seq.size()]; // assembled bytecode
  size_t pos = 0;
  for(BytecodeSeq::iterator i = seq.begin(); i!=seq.end(); 
      i++, pos++) {
    a_seq[pos].op = bytecodelookup(i->first);
    SimpleArg *sa;
    BytecodeSeq *seq_arg;
    SimpleArgAndSeq *sas;
    if ((sa = dynamic_cast<SimpleArg*>(i->second)) != NULL) {
      a_seq[pos].val = sa->getVal();
    }
    else {
      if ((seq_arg = dynamic_cast<BytecodeSeq*>(i->second)) != NULL) {
        /*Not sure if this is a good idea: thread libraries tend to
        give limited stack space.  Probably better use explicit stack,
        or better alloc things in a process's heap.
        */
        assemble(*seq_arg, a_seq[pos].seq);
      }
      else {
        if ((sas = dynamic_cast<SimpleArgAndSeq*>(i->second)) != NULL) {
          a_seq[pos].val = sas->getSimple()->getVal();
          assemble(*(sas->getSeq()), a_seq[pos].seq);
        }
        else {
          if (i->second!=NULL)
            throw_HlError("assemble: Unknown argument type");
        }
      }
    }
  }
}

/*attempts to deallocate the specified object if it's a reusable
continuation closure
*/
//static void attempt_kclos_dealloc(Heap& hp, Generic* gp) {
//	KClosure* kp = dynamic_cast<KClosure*>(gp);
//	if(kp == NULL) return;
//	if(!kp->reusable()) return;
//	hp.lifo().normal_dealloc(gp);
//}

ProcessStatus execute(Process& proc, size_t reductions, bool init){
  /*REMINDER
    All allocations of Generic objects on the
    Process proc *will* invalidate any pointers
    and references to Generic objects in that
    process.
    i.e. new(proc) Anything() will *always*
    invalidate any existing pointers.
    (technically it will only do so if a GC
    is triggered but hey, burned once, burned
    for all eternity)
    (oh and yeah: proc.nilobj() and proc.tobj()
    are both potentially allocating)
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
      ("car",			THE_BYTECODE_LABEL(car))
      ("car-local-push",	THE_BYTECODE_LABEL(car_local_push))
      ("car-clos-push",	THE_BYTECODE_LABEL(car_clos_push))
      ("cdr",			THE_BYTECODE_LABEL(cdr))
      ("cdr-local-push",	THE_BYTECODE_LABEL(cdr_local_push))
      ("cdr-clos-push",	THE_BYTECODE_LABEL(cdr_clos_push))
      ("check-vars",		THE_BYTECODE_LABEL(check_vars))
      ("closure",		THE_BYTECODE_LABEL(closure))
      ("closure-ref",		THE_BYTECODE_LABEL(closure_ref))
      //      ("composeo",		THE_BYTECODE_LABEL(composeo))
      ("cons",		THE_BYTECODE_LABEL(cons))
      ("continue",		THE_BYTECODE_LABEL(b_continue))
      ("continue-local",	THE_BYTECODE_LABEL(continue_local))
      ("continue-on-clos",	THE_BYTECODE_LABEL(continue_on_clos))
      ("global",		THE_BYTECODE_LABEL(global))
      ("global-set",		THE_BYTECODE_LABEL(global_set))
      ("halt",		THE_BYTECODE_LABEL(halt))
      ("halt-local-push",	THE_BYTECODE_LABEL(halt_local_push))
      ("halt-clos-push",	THE_BYTECODE_LABEL(halt_clos_push))
      ("if",			THE_BYTECODE_LABEL(b_if))
      ("if-local",		THE_BYTECODE_LABEL(if_local))
      ("int",			THE_BYTECODE_LABEL(b_int))
      ("k-closure",		THE_BYTECODE_LABEL(k_closure))
      ("k-closure-recreate",	THE_BYTECODE_LABEL(k_closure_recreate))
      ("k-closure-reuse",	THE_BYTECODE_LABEL(k_closure_reuse))
      ("lit-nil",		THE_BYTECODE_LABEL(lit_nil))
      ("lit-t",		THE_BYTECODE_LABEL(lit_t))
      ("local",		THE_BYTECODE_LABEL(local))
      ("reducto",		THE_BYTECODE_LABEL(reducto))
//       ("rep",			THE_BYTECODE_LABEL(rep))
//       ("rep-local-push",	THE_BYTECODE_LABEL(rep_local_push))
//       ("rep-clos-push",	THE_BYTECODE_LABEL(rep_clos_push))
//       ("sv",			THE_BYTECODE_LABEL(sv))
//       ("sv-local-push",	THE_BYTECODE_LABEL(sv_local_push))
//       ("sv-clos-push",	THE_BYTECODE_LABEL(sv_clos_push))
//       ("sv-ref",		THE_BYTECODE_LABEL(sv_ref))
//       ("sv-ref-local-push",	THE_BYTECODE_LABEL(sv_ref_local_push))
//       ("sv-ref-clos-push",	THE_BYTECODE_LABEL(sv_ref_clos_push))
//       ("sym",			THE_BYTECODE_LABEL(sym))
//       ("symeval",		THE_BYTECODE_LABEL(symeval))
//       ("tag",			THE_BYTECODE_LABEL(tag))
//       ("type",		THE_BYTECODE_LABEL(type))
//       ("type-local-push",	THE_BYTECODE_LABEL(type_local_push))
//       ("type-clos-push",	THE_BYTECODE_LABEL(type_clos_push))
      ("variadic",		THE_BYTECODE_LABEL(variadic))
      /*assign bultin global*/
      ;/*end initializer*/
    return process_running;
  }
  // main VM loop
 call_current_closure:
  if(--reductions == 0) return process_running;
  // get current closure
  // WARNING!  Will fail for KClosure.  Have to use
  // dynamic_cast<>!
  Closure & clos = *Object::_as_a<Closure*>(stack[0]);
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
      //attempt_kclos_dealloc(proc, stack[0]);
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
    BYTECODE(car): {
      /*exact implementation is in
        inc/bytecodes.hpp ; we can
        actually change the interpreter
        system.
      */
      bytecode_<&car>(stack);
    } NEXT_BYTECODE;
    BYTECODE(car_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&car>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(car_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&car>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr): {
      bytecode_cdr(stack);
    } NEXT_BYTECODE;
    BYTECODE(cdr_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&cdr>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(cdr_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&cdr>(stack, clos, N);
    } NEXT_BYTECODE;
    BYTECODE(check_vars): {
      INTPARAM(N);
      bytecode_check_vars(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(closure): {
      INTSEQPARAM(N,S);
      Closure* nclos = Closure::NewClosure(proc, S, N);
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(closure_ref): {
      INTPARAM(N);
      bytecode_closure_ref(stack, clos, N);
    } NEXT_BYTECODE;
    //    BYTECODE(composeo): {
      /*destructure closure*/
      //stack.push(clos[0]);
      //stack[0] = clos[1];
      // clos is now no longer safe to use
      //KClosure& kclos = *NewKClosure(proc, THE_BYTECODE_LABEL(composeo_continuation), 2);
      // clos is now invalid
      //kclos[0] = stack[1];
      //kclos[1] = stack.top(); stack.pop();
      //stack[1] = &kclos;
      //goto call_current_closure;
    //} NEXT_BYTECODE;
    //BYTECODE(composeo_continuation): {
    //stack.push(clos[1]);
    //stack.push(clos[0]);
    //stack.push(stack[1]);
    //attempt_kclos_dealloc(proc, stack[0]);
    //stack.restack(3);
    //goto call_current_closure;
    //} NEXT_BYTECODE;
    BYTECODE(cons): {
      bytecode_cons(proc,stack);
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
      stack.top() = clos[N];
      stack.push(gp);
      //attempt_kclos_dealloc(proc, stack[0]);
      stack.restack(2);
      goto call_current_closure;
    } NEXT_BYTECODE;
    BYTECODE(global): {
      INTPARAM(S);
      //bytecode_global(proc, stack, S);
    } NEXT_BYTECODE;
    BYTECODE(global_set): {
      INTPARAM(S);
      //bytecode_global_set(proc,stack,S);
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
      stack.push(clos[N]);
      stack.restack(1);
      return process_dead;
    } NEXT_BYTECODE;
    BYTECODE(b_if): {
      Object::ref gp = stack.top(); stack.pop();
      if (gp!=Object::nil()) {
        SEQPARAM(S);
        pc = S;
        pc--; // NEXT_BYTECODE will increment
        // is that safe??  wouldn't that have
        // array out of bounds?
      }
    } NEXT_BYTECODE;
    /*
      we expect this to be much more
      common due to CPS conversion; in
      fact we don't expect a plain 'if
      bytecode at all...
    */
    BYTECODE(if_local): {
      INTSEQPARAM(N,S);
      if(stack[N]!=Object::nil()){
        pc = S;
        pc--; // NEXT_BYTECODE will increment
      }
    } NEXT_BYTECODE;
    BYTECODE(b_int): {
      INTPARAM(N);
      bytecode_int(proc, stack, N);
    } NEXT_BYTECODE;
    BYTECODE(k_closure): {
    k_closure_perform_create:
      INTSEQPARAM(N,S);
      KClosure *nclos = KClosure::NewKClosure(proc, S, N);
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
    } NEXT_BYTECODE;
    BYTECODE(k_closure_recreate): {
      // attempt_kclos_dealloc(proc, stack[0]);
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
    BYTECODE(k_closure_reuse): {
      INTSEQPARAM(N,S);
      KClosure *nclos = expect_type<KClosure>(stack[0], "KClosure expected!");
      if(!nclos->reusable()) {
        // Use the size of the current closure
        nclos = KClosure::NewKClosure(proc, S, clos.size());
        //clos is now invalid
      } else {
        nclos->codereset(S);
      }
      for(int i = N; i ; --i){
        (*nclos)[i - 1] = stack.top();
        stack.pop();
      }
      stack.push(Object::to_ref(nclos));
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
    /*
      reducto is a bytecode to *efficiently*
      implement the common reduction functions,
      such as '+.  This bytecode avoids
      allocating new space when called with 2
      Arc arguments or less, and only allocates
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
//       if(stack.size() < 2){
//         throw_HlError("apply: Insufficient number of parameters to variadic function");
//       }
//       size_t params = stack.size() - 2;
//       if(params < 3){
//         /*simple and quick dispatch
//           for common case
//         */
//         stack[0] = clos[params];
//         /*don't disturb the other
//           parameters; the point is
//           to be efficient for the
//           common case
//         */
//       } else {
//         stack[0] = clos[2]; // f2
//         size_t saved_params =
//           params - 2;
//         KClosure & kclos = *KClosure::NewKClosure(proc, THE_BYTECODE_LABEL(reducto_continuation), saved_params + 3);
//         // clos is now invalid
//         kclos[0] = stack[0]; // f2
//         kclos[1] = stack[1];
//         /*** placeholder ***/
//         kclos[2] = stack[1];
//         for(size_t i = saved_params + 3; i > 3; --i) {
//           kclos[i - 1] =
//             stack.top();
//           stack.pop();
//         }
//         /*save closure*/
//         stack[1] = &kclos;
//         Integer* Np = proc.create<Integer>(3);
//         // kclos is now invalid
//         KClosure& kkclos = *static_cast<KClosure*>(stack[1]);
//         kkclos[2] = Np;
//       }
    } NEXT_BYTECODE;
    BYTECODE(reducto_continuation): {
//       Integer* Np = static_cast<Integer*>(clos[2]);
//       int N = Np->val;
//       int NN = N + 1; // next N
//       stack.push(clos[0]);
//       if(NN == clos.size()){
//         // final iteration
//         stack.push(clos[1]);
//         stack.push(stack[1]);
//         stack.push(clos[N]);
//         attempt_kclos_dealloc(proc, stack[0]);
//         //clos is now invalid
//         stack.restack(4);
//       } else {
//         /*dynamic_cast because it is possible for
//           the program to reconstruct a continuation
//           using an ordinary, non-continuation
//           Closure
//         */
//         KClosure* pkclos =
//           dynamic_cast<KClosure*>(&clos);
//         if(pkclos && pkclos->reusable()){
//           // a reusable continuation
//           Np->val++; // Np is also reusable
//           stack.push(pkclos);
//           stack.push(stack[1]);
//           stack.push(clos[N]);
//           stack.restack(4);
//         } else {
//           /*TODO: instead create a closure
//             with only a reference to this
//             closure and an index number, to
//             reduce memory allocation.
//           */
//           KClosure& nclos =
//             *NewKClosure(proc, THE_BYTECODE_LABEL(reducto_continuation),
//                          // save only necessary
//                          clos.size() - NN + 3);
//           // Np, pkclos and clos are now invalid
//           Closure & clos = *static_cast<Closure*>(stack[0]); //revalidate clos
//           nclos[0] = clos[0];
//           nclos[1] = clos[1];
//           /*** placeholder! ***/
//           nclos[2] = clos[1];
//           for(int j = 0;
//               j < clos.size() - NN;
//               ++j){
//             nclos[3 + j] =
//               clos[NN + j];
//           }
//           stack.push(&nclos);
//           stack.push(stack[1]);
//           stack.push(clos[N]);
//           stack.restack(4);
//           // clos is now invalid again
//           Integer* Np = proc.create<Integer>(3);
//           // nclos is now invalid
//           KClosure& kkclos = *static_cast<KClosure*>(stack[1]);
//           kkclos[2] = Np;
//         }
//       }
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
//     BYTECODE(sv): {
//       bytecode_<&Generic::make_sv>(proc, stack);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_local_push): {
//       INTPARAM(N);
//       bytecode_local_push_<&Generic::make_sv>(proc, stack, N);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_clos_push): {
//       INTPARAM(N);
//       bytecode_clos_push_<&Generic::make_sv>(proc, stack, clos, N);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_ref): {
//       bytecode_<&Generic::sv_ref>(stack);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_ref_local_push): {
//       INTPARAM(N);
//       bytecode_local_push_<&Generic::sv_ref>(stack, N);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_ref_clos_push): {
//       INTPARAM(N);
//       bytecode_clos_push_<&Generic::sv_ref>(stack, clos, N);
//     } NEXT_BYTECODE;
//     BYTECODE(sv_set): {
//       bytecode_sv_set(stack);
//     } NEXT_BYTECODE;
//     BYTECODE(sym): {
//       INTPARAM(S);
//       bytecode_sym(proc, stack, S);
//     } NEXT_BYTECODE;
//     BYTECODE(symeval): {
//       bytecode_symeval(proc, stack);
//     } NEXT_BYTECODE;
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
    } NEXT_BYTECODE;
  }
}
