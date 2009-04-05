#include"all_defines.hpp"
#include <stdlib.h> // for size_t
#include <string>
#include <sstream>
#include <new> // for std::bad_alloc
#include "types.hpp"
#include "executors.hpp"
#include "bytecodes.hpp"
#include "workers.hpp"

#ifdef DEBUG
  #include <typeinfo>
#endif
#include <iostream>

ExecutorTable Executor::tbl;

// map opcodes to bytecodes
static std::map<Symbol*,_bytecode_label> bytetb;
// inverse map for disassembly
static std::map<_bytecode_label, Symbol*> inv_bytetb;

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
			_bytecode_label l,
                        bytecode_arg_type tp = ARG_NONE) const{
                Symbol *opcode = symbols->lookup(s);
		bytetb[opcode] = l;
                inv_bytetb[l] = opcode;
                assembler.regArg(l, tp);
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
  if (code.get()==NULL) { // first instruction added
    code.reset(new bytecode_t[64]); // default initial size
    nextCode = 0;
    codeSize = 64;
  } else if (nextCode >= codeSize) {
    bytecode_t *nb = new bytecode_t[codeSize*2];
    bytecode_t *c = code.get();
    for (size_t i = 0; i<codeSize; i++)
      nb[i] = c[i];
    delete [] c;
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

// generic closure/k_closure/k_closure_recreate/k_closure_reuse 
// assembly operation
class GenClosureAs : public AsOp {
private:
  Symbol* s_to_build;
  static std::map<Symbol*, Symbol*> names;

  Symbol* operationName(Symbol *s) {
    if (names.find(s) != names.end())
      return names[s];
    else
      throw_HlError("disassembler: internal error");
  }

public:
  GenClosureAs(const char *to_build, const char *name) 
    : s_to_build(symbols->lookup(to_build)) {
    names[s_to_build] = symbols->lookup(name);
  }
  ~GenClosureAs() {
    names.erase(names.find(s_to_build));
  }

  void assemble(Process & proc);
  size_t disassemble(Process & proc, size_t i);
};

std::map<Symbol*, Symbol*> GenClosureAs::names;

void GenClosureAs::assemble(Process & proc) {
  // assemble the seq. arg, which must be on the top of the stack
  assembler.go(proc);
  Object::ref body = proc.stack.top(); proc.stack.pop();
  // number of objects to close over
  Object::ref nclose = proc.stack.top(); proc.stack.pop();
  Bytecode *current = expect_type<Bytecode>(proc.stack.top());
  size_t iconst = current->closeOver(body); // close over the body
  // reference to the body
  current->push("const-ref", iconst);
  // build the closure
  current->push(s_to_build, as_a<int>(nclose));
}

size_t GenClosureAs::disassemble(Process & proc, size_t i) {
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  bytecode_t bc = b->getCode()[i];
  Object::ref body = (*b)[bc.val];
  if (!maybe_type<Bytecode>(body)) {
    // not a closure
    // handle (float ...) bytecode here
    // !! it should be in ComplexAs::disassemble
    // !! but const-ref is already catched by GenClosureAs
    if (!maybe_type<Float>(body)) {
      throw_HlError("can't disassemble: reference to invalid object found");
    }
    proc.stack.push(body);
    proc.stack.push(Object::to_ref(proc.create<Cons>()));
    Cons *c2 = proc.create<Cons>();
    Object::ref c = proc.stack.top(); proc.stack.pop();
    scar(c, Object::to_ref(symbols->lookup("float")));
    scdr(c, Object::to_ref(c2));
    c2->scar(proc.stack.top()); proc.stack.pop(); // float value
    c2->scdr(Object::nil());
    proc.stack.push(c); // return value

    return i+1;
  }
  proc.stack.push(body);
  // disassemble the body
  assembler.goBack(proc, 0, expect_type<Bytecode>(body)->getLen());
  // stack is
  //  - body seq
  //  - current Bytecode
  b = expect_type<Bytecode>(proc.stack.top(2));
  bytecode_t next = b->getCode()[i+1];
  Symbol *opname = operationName(inv_bytetb[next.op]);
  size_t N = next.val;
  Cons *c = proc.create<Cons>();
  c->scar(Object::to_ref(opname));
  proc.stack.push(Object::to_ref(c));
  Cons *c2 = proc.create<Cons>();
  c = expect_type<Cons>(proc.stack.top()); proc.stack.pop();
  c2->scar(Object::to_ref((int)N));
  c2->scdr(proc.stack.top()); proc.stack.pop();
  c->scdr(Object::to_ref(c2));
  proc.stack.push(Object::to_ref(c));

  return i+2;
}

class ClosureAs : public GenClosureAs {
public:
  ClosureAs() : GenClosureAs("build-closure", "closure") {}
};

class KClosureAs : public GenClosureAs {
public:
  KClosureAs() : GenClosureAs("build-k-closure", "k-closure") {}
};

class KClosureRecreateAs : public GenClosureAs {
public:
  KClosureRecreateAs() : GenClosureAs("build-k-closure-recreate", 
                                      "k-closure-recreate") {}
};

class KClosureReuseAs : public GenClosureAs {
public:
  KClosureReuseAs() : GenClosureAs("build-k-closure-reuse", 
                                   "k-closure-reuse") {}
};

// assemble instructions to push a complex constant on the stack
// a complex constant is one that resides on the process-local heap
template <class T>
class ComplexAs : public AsOp {
public:
  void assemble(Process & proc);
  size_t disassemble(Process & proc, size_t i) {
    // ComplexAs disassembly is handled by GenClosureAs
    throw_HlError("disassembler error");
    return i;
  }
};

template <class T>
void ComplexAs<T>::assemble(Process & proc) {
  proc.stack.pop(); // there should be no sequence
  Object::ref arg = proc.stack.top(); proc.stack.pop();
  expect_type<T>(arg, "assemble: wrong type for complex argument");
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  if (Assembler::isComplexConst(arg)) {
    size_t i = b->closeOver(arg);
    // generate a reference to it
    b->push("const-ref", i);
  } else {
    throw_HlError("assemble: const expects a complex arg");
  }
}

// build a jmp-nil instruction out of a (if (...) (...)) op
class IfAs : public AsOp {
private:
  static size_t countToSkip(Object::ref seq);
public:
  void assemble(Process & proc);
  size_t disassemble(Process & proc, size_t i);
};

size_t IfAs::countToSkip(Object::ref seq) {
  size_t n = 0;
  for(Object::ref i = seq; i != Object::nil(); i = cdr(i)) {
    if (as_a<Symbol*>(car(car(i)))==symbols->lookup("if"))
      n += 1 + countToSkip(cdr(car(i))); // +1 for jmp-nil
    else
      n++;
  }

  return n;
}

void IfAs::assemble(Process & proc) {
  Object::ref seq = proc.stack.top(); proc.stack.pop();
  proc.stack.pop(); // throw away simple arg
  size_t to_skip = countToSkip(seq);
  // skipping instruction
  expect_type<Bytecode>(proc.stack.top())->push("jmp-nil", to_skip);
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

size_t IfAs::disassemble(Process & proc, size_t i) {
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  bytecode_t bc = b->getCode()[i];
  size_t end = i+1+bc.val;
  // duplicate Bytecode reference, 'cause goBack removes the Bytecode
  proc.stack.push(proc.stack.top()); 
  assembler.goBack(proc, i+1, end);
  Cons *c = proc.create<Cons>();
  c->scar(Object::to_ref(symbols->lookup("if")));
  c->scdr(proc.stack.top()); proc.stack.pop();
  proc.stack.push(Object::to_ref(c));

  return end;
}

Assembler assembler;

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
    case 0:
      throw_HlError("assemble: empty bytecode");
      break;
    case 1: // no args
      proc.stack.push(Object::nil());
      proc.stack.push(Object::nil());
      break;
    case 2: // simple or seq
      if (maybe_type<Cons>(car(cdr(current_op)))) { // seq
        proc.stack.push(Object::nil()); // empty simple arg
        proc.stack.push(cdr(current_op));
      } else { // simple
        proc.stack.push(car(cdr(current_op)));
        proc.stack.push(Object::nil());
      }
      break;
    default: // simple+seq or seq
      if (maybe_type<Cons>(car(cdr(current_op)))) { // just seq
        proc.stack.push(Object::nil());
        proc.stack.push(cdr(current_op));
      } else { // simple+seq
        proc.stack.push(car(cdr(current_op))); // simple
        proc.stack.push(cdr(cdr(current_op))); // seq
      }
      break;
    }
    // stack now is:
    // - seq arg
    // - simple arg
    // - current Bytecode
    // - seq being assembled
    if (!is_a<Symbol*>(car(current_op)))
      throw_HlError("assemble: symbol expected in operator position");
    Symbol *op = as_a<Symbol*>(car(current_op));
    sym_op_tbl::iterator it = tbl.find(op);
    if (it!=tbl.end()) {
      // do the call
      it->second->assemble(proc);
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
  // remove empty sequence
  proc.stack.top(2) = proc.stack.top();
  proc.stack.pop();
  // stack now is
  //  - bytecode
}

void Assembler::goBack(Process & proc, size_t start, size_t end) {
  Process::ExtraRoot head(proc);
  Process::ExtraRoot tail(proc);
  head = tail = Object::nil();

  while (start < end) {
    bytecode_t b = expect_type<Bytecode>(proc.stack.top())->getCode()[start];
    lbl_op_tbl::iterator op = inv_tbl.find(b.op);
    if (op==inv_tbl.end()) {
      // default behavior
      Cons *c = proc.create<Cons>();
      c->scar(Object::to_ref(inv_bytetb[b.op]));
      proc.stack.push(Object::to_ref(c));
      switch (argType(b.op)) {
      case ARG_INT:
        {
          Cons *c2 = proc.create<Cons>();
          c2->scar(Object::to_ref((int)b.val));
          c2->scdr(Object::nil());
          scdr(proc.stack.top(), Object::to_ref(c2));
        }
        break;
      case ARG_SYMBOL:
        {
          Cons *c2 = proc.create<Cons>();
          c2->scar(Object::to_ref((Symbol*)b.val));
          c2->scdr(Object::nil());
          scdr(proc.stack.top(), Object::to_ref(c2));      
        }
        break;
      default:
        c->scdr(Object::nil());
        break;
      }
      start++;
    } else {
      start = op->second->disassemble(proc, start);
    }

    // append instruction at the end
    if (head==Object::nil()) {
      head = Object::to_ref(proc.create<Cons>());
      scar(head, proc.stack.top()); proc.stack.pop();
      scdr(head, Object::nil());
      tail = head;
    } else {
      Cons *c = proc.create<Cons>();
      c->scar(proc.stack.top()); proc.stack.pop();
      c->scdr(Object::nil());
      scdr(tail, Object::to_ref(c));
      tail = cdr(tail);
    }
  }
  proc.stack.pop(); // remove Bytecode
  proc.stack.push(head);
}

bytecode_arg_type Assembler::argType(_bytecode_label lbl) {
  if (op_args.find(lbl)!=op_args.end())
    return op_args[lbl];
  else
    return ARG_NONE; // default to ARG_NONE
}

void Assembler::regArg(_bytecode_label lbl, bytecode_arg_type tp) {
  op_args[lbl] = tp;
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
    size_t l = as_a<int>(expect_type<Cons>(car(i))->len());
    if (l==2 && isComplexConst(car(cdr(car(i)))))
      n++;
    if (l>2) {
      if (as_a<Symbol*>(car(car(i)))==symbols->lookup("if")) {
        // must look within the 'if subseq
        n += countConsts(cdr(car(i)));
      } else {
        n++;
      }
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

#define SETCLOS(name) name = expect_type<Closure>(stack[0], "internal: SETCLOS expects a Closure in stack[0]")

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
      ("acquire",		THE_BYTECODE_LABEL(acquire))
      ("apply",		THE_BYTECODE_LABEL(apply), ARG_INT)
      ("apply-invert-k",	THE_BYTECODE_LABEL(apply_invert_k), ARG_INT)
      ("apply-k-release",	THE_BYTECODE_LABEL(apply_k_release), ARG_INT)
      ("apply-list",		THE_BYTECODE_LABEL(apply_list), ARG_INT)
      ("build-closure", THE_BYTECODE_LABEL(build_closure), ARG_INT)
      ("build-k-closure", THE_BYTECODE_LABEL(build_k_closure), ARG_INT)
      ("build-k-closure-recreate",THE_BYTECODE_LABEL(build_k_closure_recreate),
       ARG_INT)
      ("build-k-closure-reuse",	THE_BYTECODE_LABEL(build_k_closure_reuse), 
       ARG_INT)
      ("car",			THE_BYTECODE_LABEL(car))
      ("scar",                  THE_BYTECODE_LABEL(scar))
      ("car-local-push",	THE_BYTECODE_LABEL(car_local_push), ARG_INT)
      ("car-clos-push",	THE_BYTECODE_LABEL(car_clos_push), ARG_INT)
      ("cdr",			THE_BYTECODE_LABEL(cdr))
      ("scdr",                  THE_BYTECODE_LABEL(scdr))
      ("cdr-local-push",	THE_BYTECODE_LABEL(cdr_local_push), ARG_INT)
      ("cdr-clos-push",	THE_BYTECODE_LABEL(cdr_clos_push), ARG_INT)
      ("char",			THE_BYTECODE_LABEL(b_char), ARG_INT)
      ("check-vars",		THE_BYTECODE_LABEL(check_vars), ARG_INT)
      ("closure-ref",		THE_BYTECODE_LABEL(closure_ref), ARG_INT)
      ("composeo",		THE_BYTECODE_LABEL(composeo))
      ("composeo-continuation",	THE_BYTECODE_LABEL(composeo_continuation))
      ("cons",		THE_BYTECODE_LABEL(cons))
      ("const-ref",     THE_BYTECODE_LABEL(const_ref), ARG_INT)
      ("continue",		THE_BYTECODE_LABEL(b_continue))
      ("continue-local",	THE_BYTECODE_LABEL(continue_local), ARG_INT)
      ("continue-on-clos",	THE_BYTECODE_LABEL(continue_on_clos), ARG_INT)
      ("f-to-i",		THE_BYTECODE_LABEL(f_to_i))
      ("global",		THE_BYTECODE_LABEL(global), ARG_SYMBOL)
      ("global-set",		THE_BYTECODE_LABEL(global_set), ARG_SYMBOL)
      ("halt",		THE_BYTECODE_LABEL(halt))
      ("halt-local-push",	THE_BYTECODE_LABEL(halt_local_push), ARG_INT)
      ("halt-clos-push",	THE_BYTECODE_LABEL(halt_clos_push), ARG_INT)
      ("i-to-f",		THE_BYTECODE_LABEL(i_to_f))
      ("jmp-nil",		THE_BYTECODE_LABEL(jmp_nil), ARG_INT) // replacement for 'if
      //("if-local",		THE_BYTECODE_LABEL(if_local))
      ("int",			THE_BYTECODE_LABEL(b_int), ARG_INT)
      ("ccc", THE_BYTECODE_LABEL(ccc))
      ("lit-nil",		THE_BYTECODE_LABEL(lit_nil))
      ("lit-t",		THE_BYTECODE_LABEL(lit_t))
      ("local",		THE_BYTECODE_LABEL(local), ARG_INT)
      ("monomethod",		THE_BYTECODE_LABEL(monomethod))
      ("reducto",		THE_BYTECODE_LABEL(reducto))
      ("<bc>recv", THE_BYTECODE_LABEL(recv))
      ("reducto-continuation",   THE_BYTECODE_LABEL(reducto_continuation))
      ("release",		THE_BYTECODE_LABEL(release))
      ("rep",			THE_BYTECODE_LABEL(rep))
      ("rep-local-push",	THE_BYTECODE_LABEL(rep_local_push))
      ("rep-clos-push",	THE_BYTECODE_LABEL(rep_clos_push))
      ("<bc>self-pid", THE_BYTECODE_LABEL(self_pid))
      ("<bc>send", THE_BYTECODE_LABEL(send))
      ("<bc>spawn", THE_BYTECODE_LABEL(spawn))
      ("string-create",		THE_BYTECODE_LABEL(string_create), ARG_INT)
      ("string-length",		THE_BYTECODE_LABEL(string_length))
      ("string-ref",		THE_BYTECODE_LABEL(string_ref))
      ("string-sref",		THE_BYTECODE_LABEL(string_sref))
      ("sv",			THE_BYTECODE_LABEL(sv))
      ("sv-local-push",	THE_BYTECODE_LABEL(sv_local_push), ARG_INT)
      ("sv-clos-push",	THE_BYTECODE_LABEL(sv_clos_push), ARG_INT)
      ("sv-ref",		THE_BYTECODE_LABEL(sv_ref))
      ("sv-ref-local-push",    THE_BYTECODE_LABEL(sv_ref_local_push), ARG_INT)
      ("sv-ref-clos-push",	THE_BYTECODE_LABEL(sv_ref_clos_push), ARG_INT)
      ("sv-set",		THE_BYTECODE_LABEL(sv_set))
      ("sym",			THE_BYTECODE_LABEL(sym), ARG_SYMBOL)
      ("symeval",		THE_BYTECODE_LABEL(symeval), ARG_SYMBOL)
      ("table-create",		THE_BYTECODE_LABEL(table_create))
      ("table-ref",		THE_BYTECODE_LABEL(table_ref))
      ("table-sref",		THE_BYTECODE_LABEL(table_sref))
      ("table-keys",		THE_BYTECODE_LABEL(table_keys))
      ("tag",			THE_BYTECODE_LABEL(tag))
      ("<bc>try-recv", THE_BYTECODE_LABEL(try_recv))
      ("type",		THE_BYTECODE_LABEL(type))
      ("type-local-push",	THE_BYTECODE_LABEL(type_local_push))
      ("type-clos-push",	THE_BYTECODE_LABEL(type_clos_push))
      ("variadic",		THE_BYTECODE_LABEL(variadic), ARG_INT)
      ("do-executor",           THE_BYTECODE_LABEL(do_executor), ARG_SYMBOL)
      ("i+",                    THE_BYTECODE_LABEL(iplus))
      ("i-",                    THE_BYTECODE_LABEL(iminus))
      ("i*",                    THE_BYTECODE_LABEL(imul))
      ("i/",                    THE_BYTECODE_LABEL(idiv))
      ("imod",			THE_BYTECODE_LABEL(imod))
      ("i<",                    THE_BYTECODE_LABEL(iless))
      ("f+",                    THE_BYTECODE_LABEL(fplus))
      ("f-",                    THE_BYTECODE_LABEL(fminus))
      ("f*",                    THE_BYTECODE_LABEL(fmul))
      ("f/",                    THE_BYTECODE_LABEL(fdiv))
      ("f<",                    THE_BYTECODE_LABEL(fless))
      /*declare executors*/
      ("is-symbol-packaged",	THE_EXECUTOR<IsSymbolPackaged>())
      /*assign bultin global*/
      ;/*end initializer*/

    // initialize assembler operations
    assembler.reg<ClosureAs>(symbols->lookup("closure"),
                             THE_BYTECODE_LABEL(const_ref));
    assembler.reg<KClosureAs>(symbols->lookup("k-closure"), NULL);
    assembler.reg<KClosureRecreateAs>(symbols->lookup("k-closure-recreate"),
                                      NULL);
    assembler.reg<KClosureReuseAs>(symbols->lookup("k-closure-reuse"), NULL);
    assembler.reg<IfAs>(symbols->lookup("if"), THE_BYTECODE_LABEL(jmp_nil));
    assembler.reg<ComplexAs<Float> >(symbols->lookup("float"), NULL);

    /*
     * build and assemble various bytecode sequences
     * these will hold a fixed Bytecodes that will be used
     * as the continuations for various bytecodes
     */
    symbols->lookup("<impl>reducto-cont-body")->
      set_value(inline_assemble(proc, "(reducto-continuation) (continue)"));
    symbols->lookup("<impl>ccc-fn-body")->
      set_value(inline_assemble(proc, "(check-vars 3) (continue-on-clos 0)"));
    symbols->lookup("<impl>composeo-cont-body")->
      set_value(inline_assemble(proc, "(composeo-continuation ) (continue )"));

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
  // TODO: in the future, replace below with maybe_type<Closure>;
  // if not a Closure, get value for <axiom>call* and manipulate
  // stack
  Closure *clos = expect_type<Closure>(stack[0], "execute: closure expected!");
  // a reference to the current bytecode *must* be retained for 
  // k-closure-recreate and k-closure-reuse to work correctly:
  // they may change the body of the current closure, but we are still
  // executing the old body and if we don't retain it we can't access it from
  // clos->code(), since clos->code() may now refer to a different body
  bytecode = clos->code();
  // to start, call the closure in stack[0]
  DISPATCH_BYTECODES {
    BYTECODE(acquire): {
      proc.global_acquire();
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
        //clos is now invalid
        SETCLOS(clos);
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
      INTPARAM(N);
      bytecode_clos_push_<&car>(stack, *clos, N);
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
      INTPARAM(N);
      bytecode_clos_push_<&cdr>(stack, *clos, N);
    } NEXT_BYTECODE;
    BYTECODE(b_char): {
      INTPARAM(N);
      bytecode_char(stack, N);
    }
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
      // !! should really avoid SymbolsTable::lookup() due to
      // !! increased lock contention
      kclos.codereset(proc.global_read(symbols->lookup("<impl>composeo-cont-body")));
      // clos is now invalid
      /*continuation*/
      kclos[0] = stack[1];
      /*next function*/
      kclos[1] = stack.top(); stack.pop();
      stack[1] = Object::to_ref(&kclos);
      // this will revalidate clos
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(composeo_continuation): {
      stack.push((*clos)[1]);
      stack.push((*clos)[0]);
      stack.push(stack[1]);
      attempt_kclos_dealloc(proc, as_a<Generic*>(stack[0]));
      stack.restack(3);
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(cons): {
      bytecode_cons(proc,stack);
      SETCLOS(clos);
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
      /***/ DOCALL(); /***/
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(f_to_i): {
      bytecode_<&f_to_i>(stack);
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
      std::cerr<<"halt\n";
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
    BYTECODE(i_to_f): {
      bytecode_<&i_to_f>(proc, stack);
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    // call current continuation
    BYTECODE(recv): {
      Object::ref msg;
      if (proc.extract_message(msg)) {
        std::cerr<<"recv: "<<msg<<"\n";
        stack.push(stack[1]); // current continuation
        stack.push(msg);
        stack.restack(2);
        DOCALL();
      } else {
        std::cerr<<"recv: queue empty\n";
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
        // !! should really avoid SymbolsTable::lookup() due to
        // !! increased lock contention
        kclos.codereset(proc.global_read(symbols->lookup("<impl>reducto-cont-body")));
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
      /***/ DOCALL(); /***/
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
          // !! should really avoid SymbolsTable::lookup() due to
          // !! increased lock contention
          nclos.codereset(proc.global_read(symbols->lookup("<impl>reducto-cont-body")));
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
      /***/ DOCALL(); /***/
    } NEXT_BYTECODE;
    BYTECODE(release): {
      /*On this machine, <bc>release does nothing*/
    } NEXT_BYTECODE;
    BYTECODE(rep): {
      bytecode_<rep>(stack);
    } NEXT_BYTECODE;
    BYTECODE(rep_local_push): {
      INTPARAM(N);
      bytecode_local_push_<rep>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(rep_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<rep>(stack, *clos, N);
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
      Object::ref msg = proc.stack.top(); proc.stack.pop();
      HlPid *pid = expect_type<HlPid>(proc.stack.top(), "send expects a pid as first argument");
      proc.stack.pop();
      ValueHolderRef ref;
      ValueHolder::copy_object(ref, msg);
      bool is_waiting = false;
      if (!pid->process->receive_message(ref, is_waiting)) {
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
    // call current continuation, passing the pid of created process
    BYTECODE(spawn): {
      std::cerr << "spawning\n";
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
      std::cerr << "spawned\n";
      return process_change;
    } NEXT_BYTECODE;
    BYTECODE(string_create): {
      INTPARAM(N); // length of string to create from stack
      bytecode_string_create( proc, stack, N );
      SETCLOS(clos); // allocation may invalidate clos
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
      SETCLOS(clos); // allocation may invalidate clos
    } NEXT_BYTECODE;
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
    BYTECODE(table_keys): {
      bytecode_table_keys(proc, stack);
    } NEXT_BYTECODE;
    // expects two continuations on the stack
    // one is called if there is a message, the other if 
    // the message queue is empty
    // stack is:
    // -- top --
    // fail cont
    // success cont
    // -- bottom --
    BYTECODE(try_recv): {
	// directly use the mailbox, to avoid blocking
        MailBox &mbox = proc.mailbox();
	Object::ref msg;
	// !! TODO: recv may still block on the MailBox mutex
	// ?? we can specify that <axiom>try-recv could cause
	// ?? the function it is in to be restarted, and as
	// ?? such should be protected by its own function.
	// ?? we can then use a trylock and return a bool pair
	if (mbox.recv(msg)) {
		// success
		// ?? maybe better to use CPS so that continuation
		// ?? from stack[1] is passed to a non-continuation
		// ?? function, i.e.
		// ?? stack.push(stack.top(2));
		// ?? stack.push(stack[1]);
		// ?? stack.push(msg);
		// ?? stack.restack(3);
		stack.pop(); // throw away fail cont
		stack.push(msg);
		stack.restack(2);
        } else {
		// fail
		Object::ref fail_fn = stack.top(); stack.pop();
		stack.pop(); // remove success cont
		stack.push(fail_fn); // back in the stack
		stack.push(Object::nil()); // continuations take an arg
		stack.restack(2);
        }
	DOCALL();
    } NEXT_BYTECODE;
    BYTECODE(type): {
      bytecode_<&type>(stack);
    } NEXT_BYTECODE;
    BYTECODE(type_local_push): {
      INTPARAM(N);
      bytecode_local_push_<&type>(stack, N);
    } NEXT_BYTECODE;
    BYTECODE(type_clos_push): {
      INTPARAM(N);
      bytecode_clos_push_<&type>(stack, *clos, N);
    } NEXT_BYTECODE;
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
