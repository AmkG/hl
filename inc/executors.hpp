#ifndef EXECUTORS_H
#define EXECUTORS_H

#include "processes.hpp"
#include "objects.hpp"
#include "reader.hpp" // for Bytecode classes
#include "types.hpp"

#include <map>
#include <set>

#include <boost/smart_ptr.hpp>

/*
  hl functions are represented by a Closure structure, which has an
  attached bytecode_t array.

  Calling convention:

  proc.stack[0] *must* be a Closure
  !! for now, at least.  in the future we will change this so that
  !! calls to non-Closure objects will be transformed to calls to
  !! the global <axiom>call*, which *should* be a Closure
  !! -- almkglor

  proc.stack[0] contains the closure.  If it is a "normal" 
  function, proc.stack[1] is the "continuation" function 
  and proc.stack[2..n] are the parameters.  
  proc.stack.size() is the number of parameters including 
  the function itself and the continuation.
  
  If it is a "continuation" function, proc.stack[1] is always 
  the return value. proc.stack.size() will be 2.

  Functions never return.
*/

#define PASTE_SYMBOLS(x,y) x##y

/*
  Not necessary when using GCC.
  NOTE!  We retain this and even add references to
  an _e_bytecode_label type so that modders who
  happen to use *only* GCC will remember to update
  the declarations for the benefit of non-GCC
  users.
*/

#define DECLARE_BYTECODES enum _e_bytecode_label {
#define BYTECODE_ENUM(x) PASTE_SYMBOLS(_bytecode_, x)
#define A_BYTECODE(x) BYTECODE_ENUM(x),
#define END_DECLARE_BYTECODES __null_bytecode };
DECLARE_BYTECODES
	A_BYTECODE(acquire)
	A_BYTECODE(add_event)
	A_BYTECODE(apply)
	A_BYTECODE(apply_invert_k)
	A_BYTECODE(apply_list)
	A_BYTECODE(apply_k_release)
        A_BYTECODE(b_ref)
	A_BYTECODE(bounded)
	A_BYTECODE(build_k_closure)
	A_BYTECODE(build_k_closure_recreate)
	A_BYTECODE(build_k_closure_reuse)
	A_BYTECODE(car)
	A_BYTECODE(car_local_push)
	A_BYTECODE(car_clos_push)
	A_BYTECODE(ccc)
	A_BYTECODE(cdr)
	A_BYTECODE(cdr_local_push)
	A_BYTECODE(cdr_clos_push)
	A_BYTECODE(b_char)
	A_BYTECODE(check_vars)
	A_BYTECODE(closure_ref)
	A_BYTECODE(composeo)
	A_BYTECODE(composeo_continuation)
	A_BYTECODE(cons)
        A_BYTECODE(const_ref)
	A_BYTECODE(b_continue)
	A_BYTECODE(continue_local)
	A_BYTECODE(continue_on_clos)
	A_BYTECODE(debug_call)
	A_BYTECODE(debug_tail_call)
	A_BYTECODE(debug_cont_call)
	A_BYTECODE(debug_backtrace)
	A_BYTECODE(disclose)
	A_BYTECODE(empty_event_set)
	A_BYTECODE(enclose)
	A_BYTECODE(event_poll)
	A_BYTECODE(event_wait)
	A_BYTECODE(f_to_i)
	A_BYTECODE(global)
	A_BYTECODE(global_set)
	A_BYTECODE(halt)
	A_BYTECODE(halt_local_push)
	A_BYTECODE(halt_clos_push)
	A_BYTECODE(i_to_f)
	A_BYTECODE(io_accept)
	A_BYTECODE(io_appendfile)
	A_BYTECODE(io_close)
	A_BYTECODE(io_connect)
	A_BYTECODE(io_fsync)
	A_BYTECODE(io_infile)
	A_BYTECODE(io_listener)
	A_BYTECODE(io_outfile)
	A_BYTECODE(io_pipe)
	A_BYTECODE(io_read)
	A_BYTECODE(io_seek)
	A_BYTECODE(io_stderr)
	A_BYTECODE(io_stdin)
	A_BYTECODE(io_stdout)
	A_BYTECODE(io_tell)
	A_BYTECODE(io_write)
	A_BYTECODE(is)
	A_BYTECODE(jmp_nil)
  //A_BYTECODE(if_local)
	A_BYTECODE(b_int)
        A_BYTECODE(l_to_b)
	A_BYTECODE(lit_nil)
	A_BYTECODE(lit_t)
	A_BYTECODE(local)
	A_BYTECODE(monomethod)
	A_BYTECODE(only_running)
        A_BYTECODE(recv)
	A_BYTECODE(reducto)
        A_BYTECODE(reducto_continuation)
	A_BYTECODE(release)
	A_BYTECODE(remove_event)
	A_BYTECODE(rep)
	A_BYTECODE(rep_local_push)
	A_BYTECODE(rep_clos_push)
	A_BYTECODE(scar)
	A_BYTECODE(scdr)
	A_BYTECODE(self_pid)
        A_BYTECODE(send)
        A_BYTECODE(sleep)
        A_BYTECODE(spawn)
	A_BYTECODE(string_create)
	A_BYTECODE(string_length)
	A_BYTECODE(string_ref)
	A_BYTECODE(string_sref)
	A_BYTECODE(sv)
	A_BYTECODE(sv_local_push)
	A_BYTECODE(sv_clos_push)
	A_BYTECODE(sv_ref)
	A_BYTECODE(sv_ref_local_push)
	A_BYTECODE(sv_ref_clos_push)
	A_BYTECODE(sv_set)
	A_BYTECODE(sym)
	A_BYTECODE(symeval)
        A_BYTECODE(system)
	A_BYTECODE(table_create)
	A_BYTECODE(table_keys)
	A_BYTECODE(table_ref)
	A_BYTECODE(table_sref)
	A_BYTECODE(tag)
        A_BYTECODE(try_recv)
	A_BYTECODE(type)
	A_BYTECODE(type_local_push)
	A_BYTECODE(type_clos_push)
	A_BYTECODE(variadic)
	/*maybe organize by alphabetical order of bytecodes ^^*/
	A_BYTECODE(do_executor)
	A_BYTECODE(iplus)
	A_BYTECODE(iminus)
	A_BYTECODE(imul)
	A_BYTECODE(idiv)
	A_BYTECODE(imod)
	A_BYTECODE(iless)
	A_BYTECODE(fplus)
	A_BYTECODE(fminus)
	A_BYTECODE(fmul)
	A_BYTECODE(fdiv)
	A_BYTECODE(fless)
END_DECLARE_BYTECODES

#ifdef BYTECODE_DEBUG
#include<iostream>
#define COLON_POST_BYTECODE_LABEL(x) : \
	std::cerr << "bytecode " #x << std::endl; PASTE_SYMBOLS(post_label_b_, x)
#else
#define COLON_POST_BYTECODE_LABEL(x)
#endif

#ifdef __GNUC__

// use indirect goto when using GCC

typedef void* _bytecode_label;
#define DISPATCH_BYTECODES \
        bytecode_t *pc = known_type<Bytecode>(clos->code())->getCode();\
	goto *(pc->op);
#define NEXT_BYTECODE goto *((++pc)->op)
#define BYTECODE(x) BYTECODE_ENUM(x); PASTE_SYMBOLS(label_b_, x) COLON_POST_BYTECODE_LABEL(x)
#define THE_BYTECODE_LABEL(x) &&PASTE_SYMBOLS(label_b_, x)

#else // __GNUC__

// use an enum when using standard C

typedef enum _e_bytecode_label _bytecode_label;
#define DISPATCH_BYTECODES \
	bytecode_t *pc = known_type<Bytecode>(clos->code())->getCode();\
	switch(pc->op)
#define NEXT_BYTECODE {pc++; continue;}
#define BYTECODE(x) case BYTECODE_ENUM(x) COLON_POST_BYTECODE_LABEL(x)
#define THE_BYTECODE_LABEL(x) BYTECODE_ENUM(x)

#endif // __GNUC__

class Process;
class ProcessStack;

class Executor;

class ExecutorTable : public std::map<Symbol*, Executor*> {
public:
	~ExecutorTable();
};

/* 
 * Generic executor
 * An executor represents a built-in function
 */
class Executor {
private:
  // table of available executors
  static ExecutorTable tbl;
public:
  // register an executor in the system
  // no locks: executors should be registered only during startup
  static void reg(Symbol *s, Executor *e) {
    tbl[s] = e;
  }
  // no locks: tbl stucture is immutable after initialization
  static Executor* findExecutor(Symbol *s) {
    std::map<Symbol*,Executor*>::iterator it = tbl.find(s);
    if (it==tbl.end())
      return NULL;
    else
      return it->second;
  }
  virtual ~Executor() {}
  // return true if a function call must be performed, false otherwise
  virtual bool run(Process & proc, size_t & reductions) = 0;
};

inline ExecutorTable::~ExecutorTable() {
	for(iterator i = begin(); i != end(); ++i) {
		delete i->second;
	}
}

/*
 * The bytecode read by the reader should be assembled before execution
 * an assembled bytecode is an array of bytecode_t
 * The executor can assume that the bytecodes are correct
 * The assembler must have already catched eventual errors
 */
struct bytecode_t {
  _bytecode_label op; // operation code
  intptr_t val; // simple value argument (may be invalid)
};

// type of bytecode_t::val
enum bytecode_arg_type { ARG_NONE, ARG_INT, ARG_SYMBOL };

// a bytecode object contains a table of complex constants such as gensyms
class Bytecode : public GenericDerivedVariadic<Bytecode> {
private:
  boost::shared_array<bytecode_t> code;
  size_t codeSize;
  size_t nextCode; // next free position in code
  size_t nextPos; // next free position in variadic space

public:
  Bytecode(size_t sz) 
    : GenericDerivedVariadic<Bytecode>(sz), codeSize(0), nextCode(0), 
      nextPos(0) {}
  virtual ~Bytecode() {}

  Object::ref& operator[](size_t i) {
    if (i < size())
      return index(i);
    else
      throw_HlError("internal: trying to access objects in bytecode with an index too large!");
  }

  Object::ref type(void) const {
    return Object::to_ref(symbol_bytecode);
  }

  bytecode_t* getCode() { return code.get(); }

  size_t getLen() const { return nextCode; }

  // close a complex constants
  size_t closeOver(Object::ref obj) {
    if (nextPos >= size())
      throw_HlError("internal: trying to close over, but there is no space left in Bytecode");
    index(nextPos) = obj;
    return nextPos++;
  }

  // add a bytecode at the end of the sequence
  void push(bytecode_t b);
  void push(_bytecode_label op, intptr_t val);
  void push(Symbol *s, intptr_t val);
  void push(const char *s, intptr_t val);

  void traverse_references(GenericTraverser *gt) {
    for(size_t i = 0; i < nextPos; ++i) {
      gt->traverse(index(i));
    }
  }
};

#ifdef BYTECODE_DEBUG
#define INTPARAM(name) intptr_t name = pc->val; std::cerr << "parm " << name << std::endl
#define SYMPARAM(name) Symbol *name = (Symbol*)pc->val; std::cerr << "parm " << name->getPrintName() << std::endl
#else
#define INTPARAM(name) intptr_t name = pc->val
#define SYMPARAM(name) Symbol *name = (Symbol*)pc->val
#endif

// Execute a given process
ProcessStatus execute(Process & proc, size_t& reductions, Process*& Q, bool init = 0);

#endif // EXECUTORS_H
