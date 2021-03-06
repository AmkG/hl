#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "executors.hpp"

// map opcodes to bytecodes
extern std::map<Symbol*,_bytecode_label> bytetb; // declared in executors.cpp
// inverse map for disassembly
extern std::map<_bytecode_label, Symbol*> inv_bytetb; // declared in executors.cpp

class AsOp {
public:
  // expect arguments on the stack:
  //  - sequence arg (stack top)
  //  - simple arg
  //  - Bytecode object being assembled
  //  - seq being assembled
  virtual void assemble(Process & proc) = 0;
  // expect on the stack:
  // - Bytecode object
  // i is the index of the instruction to disassemble
  // return the index of the next instruction to disassemble
  // leave disassembled instruction on the stack
  virtual size_t disassemble(Process & proc, size_t i) = 0;
  // return the number of bytecodes generated by this operation
  virtual size_t n_bytecodes() { return 1; }
};

// assembler operations
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
  size_t n_bytecodes() { return 2; }
};

class ClosureAs : public GenClosureAs {
public:
  ClosureAs() : GenClosureAs("<bc>build-closure", "<bc>closure") {}
};

class KClosureAs : public GenClosureAs {
public:
  KClosureAs() : GenClosureAs("<bc>build-k-closure", "<bc>k-closure") {}
};

class KClosureRecreateAs : public GenClosureAs {
public:
  KClosureRecreateAs() : GenClosureAs("<bc>build-k-closure-recreate", 
                                      "<bc>k-closure-recreate") {}
};

class KClosureReuseAs : public GenClosureAs {
public:
  KClosureReuseAs() : GenClosureAs("<bc>build-k-closure-reuse", 
                                   "<bc>k-closure-reuse") {}
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

// build a jmp-nil instruction out of a (if (...) (...)) op
class IfAs : public AsOp {
private:
  size_t countToSkip(Object::ref seq);
public:
  void assemble(Process & proc);
  size_t disassemble(Process & proc, size_t i);
};

// associate debug information to the current Bytecode
// parameterized on the information setter function
template <void (*S)(Bytecode *b, Object::ref info)>
class DbgInfoAs : public AsOp {
public:
	size_t disassemble(Process & proc, size_t i) { return i+1; }
	virtual size_t n_bytecodes() { return 0; }

	void assemble(Process & proc);
};

template <void (*S)(Bytecode *b, Object::ref info)>
void DbgInfoAs<S>::assemble(Process & proc) {
	proc.stack.top(); proc.stack.pop(); // no seq arg
	Object::ref info = proc.stack.top(); proc.stack.pop();
	// set the debug information
	(*S)(expect_type<Bytecode>(proc.stack.top()), info);
}

class Assembler {
private:
  typedef std::map<Symbol*, AsOp*> sym_op_tbl;
  typedef std::map<_bytecode_label, AsOp*> lbl_op_tbl;
  sym_op_tbl tbl;
  lbl_op_tbl inv_tbl;

  // extracts a value pointer/immediate object, throwing away the type tag
  static intptr_t simpleVal(Object::ref sa);

  std::map<_bytecode_label, bytecode_arg_type> op_args;
  // tells the argument type of the given bytecode
  bytecode_arg_type argType(_bytecode_label lbl);

public:
  ~Assembler() { 
    for (sym_op_tbl::iterator i = tbl.begin(); i!=tbl.end(); i++)
      delete i->second;
  }

  // register opcode as an opcode that accept an argument of the given type
  void regArg(_bytecode_label lbl, bytecode_arg_type tp);

  // register a new assembler operation
  template <class T>
  void reg(Symbol* s, _bytecode_label lbl) { 
    T *op = new T();
    tbl[s] = op;
    inv_tbl[lbl] = op;
  }

  AsOp* get_operation(Symbol *s);

  // do the assembly, leave a Bytecode on the stack, expect a sequence on 
  // the stack
  void go(Process & proc); 
  // disassemble a sequence
  // take a Bytecode from the stack, leave a sequence on the stack
  // disassemble form index start to index end (exclusive)
  void goBack(Process & proc, size_t start, size_t end);

  // count number of comples constants in seq (not recursive)
  static size_t countConsts(Object::ref seq);
  // tells if an object is a complex one or not
  static bool isComplexConst(Object::ref obj);

  static Object::ref inline_assemble(Process & proc, const char *code);
};

/*
 * Bridge between hl and the bytecode assembler
 */
class AssemblerExecutor : public Executor {
public:
	// leave a closure on proc.stack
	virtual bool run(Process & proc, size_t & reductions);
};

extern Assembler assembler;

template <class T>
void ComplexAs<T>::assemble(Process & proc) {
  proc.stack.pop(); // there should be no sequence
  Object::ref arg = proc.stack.top(); proc.stack.pop();
  expect_type<T>(arg, "assemble: wrong type for complex argument");
  Bytecode *b = expect_type<Bytecode>(proc.stack.top());
  if (Assembler::isComplexConst(arg)) {
    size_t i = b->closeOver(arg);
    // generate a reference to it
    b->push("<bc>const-ref", i);
  } else {
    throw_HlError("assemble: const expects a complex arg");
  }
}

#endif // ASSEMBLER_H
