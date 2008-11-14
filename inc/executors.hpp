#ifndef EXECUTORS_H
#define EXECUTORS_H

//#include "processes.hpp"
#include "reader.hpp" // for Bytecode classes

/*
  hl functions are represented by a Closure structure, which has an
  attached bytecode_t array.

  Calling convention:

  proc.stack[0] *must* be a Closure

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
	A_BYTECODE(apply)
	A_BYTECODE(apply_invert_k)
	A_BYTECODE(apply_list)
	A_BYTECODE(apply_k_release)
	A_BYTECODE(car)
	A_BYTECODE(car_local_push)
	A_BYTECODE(car_clos_push)
	A_BYTECODE(cdr)
	A_BYTECODE(cdr_local_push)
	A_BYTECODE(cdr_clos_push)
	A_BYTECODE(check_vars)
	A_BYTECODE(closure)
	A_BYTECODE(closure_ref)
	A_BYTECODE(composeo)
	A_BYTECODE(cons)
	A_BYTECODE(b_continue)
	A_BYTECODE(continue_local)
	A_BYTECODE(continue_on_clos)
	A_BYTECODE(global)
	A_BYTECODE(global_set)
	A_BYTECODE(halt)
	A_BYTECODE(halt_local_push)
	A_BYTECODE(halt_clos_push)
	A_BYTECODE(b_if)
	A_BYTECODE(if_local)
	A_BYTECODE(b_int)
	A_BYTECODE(k_closure)
	A_BYTECODE(k_closure_recreate)
	A_BYTECODE(k_closure_reuse)
	A_BYTECODE(lit_nil)
	A_BYTECODE(lit_t)
	A_BYTECODE(local)
	A_BYTECODE(reducto)
	A_BYTECODE(rep)
	A_BYTECODE(rep_local_push)
	A_BYTECODE(rep_clos_push)
	A_BYTECODE(sv)
	A_BYTECODE(sv_local_push)
	A_BYTECODE(sv_clos_push)
	A_BYTECODE(sv_ref)
	A_BYTECODE(sv_ref_local_push)
	A_BYTECODE(sv_ref_clos_push)
	A_BYTECODE(sv_set)
	A_BYTECODE(sym)
	A_BYTECODE(symeval)
	A_BYTECODE(tag)
	A_BYTECODE(type)
	A_BYTECODE(type_local_push)
	A_BYTECODE(type_clos_push)
	A_BYTECODE(variadic)
END_DECLARE_BYTECODES

#ifdef __GNUC__

// use indirect goto when using GCC

typedef void* _bytecode_label;
#define DISPATCH_BYTECODES \
        bytecode_t *pc = static_cast<Closure*>(stack[0])->code(); \
	goto (*(pc->op));
#define NEXT_BYTECODE goto (*((++pc)->op))
#define BYTECODE(x) PASTE_SYMBOLS(label_b_, x)
#define THE_BYTECODE_LABEL(x) &&PASTE_SYMBOLS(label_b_, x)

#else // __GNUC__

// use an enum when using standard C

typedef enum _e_bytecode_label _bytecode_label;
#define DISPATCH_BYTECODES \
	bytecode_t *pc = static_cast<Closure*>(stack[0])->code();\
	switch(pc->op)
#define NEXT_BYTECODE {pc++; continue;}
#define BYTECODE(x) case BYTECODE_ENUM(x)
#define THE_BYTECODE_LABEL(x) BYTECODE_ENUM(x)

#endif // __GNUC__

class Process;
class ProcessStatus;
class ProcessStack;

/* 
 * Generic executor
 * An executor represents a built-in function
 */
class Executor {
public:
  virtual ~Executor() {}
  virtual void run(ProcessStack & stack, size_t & reductions) = 0;
};

/*
 * The bytecode read by the reader should be assembled before execution
 * an assembled bytecode is an array of bytecode_t
 * The executor can assume that the bytecodes are correct
 * The assembler must have already catched eventual errors
 */
struct bytecode_t {
  _bytecode_label op; // operation code
  intptr_t val; // simple value argument (may be invalid)
  bytecode_t *seq; // sequence argument (may be invalid)
};

#define INTPARAM(name) intptr_t name = pc->val
#define INTSEQPARAM(name1, name2)\
        intptr_t name1 = pc->val; \
        bytecode_t* name_2 = pc->seq;


// Assemble a sequence of bytecodes
void assemble(BytecodeSeq & seq, bytecode_t* & a_seq);

// Execute a given process
ProcessStatus execute(Process & proc, size_t reductions, bool init = 0);

#endif // EXECUTORS_H