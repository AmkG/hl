#include "all_defines.hpp"
#include "types.hpp"
#include "assembler.hpp"

#include <sstream>

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
  current->push("<bc>const-ref", iconst);
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
    scar(c, Object::to_ref(symbols->lookup("<bc>float")));
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

size_t IfAs::countToSkip(Object::ref seq) {
	size_t n = 0;

	for(Object::ref i = seq; i != Object::nil(); i = cdr(i)) {
		Symbol *op = as_a<Symbol*>(car(car(i)));
		if (op==symbols->lookup("<bc>if"))
			n += this->n_bytecodes() + countToSkip(cdr(car(i)));
		else {
			AsOp *o = assembler.get_operation(op);
			if (o) {
				n += o->n_bytecodes();
			} else {
				// unknown operation
				// keep going, error will be raised when
				// this operations is assembled
				n++;
			}
		}
	}
	
	return n;
}

void IfAs::assemble(Process & proc) {
  Object::ref seq = proc.stack.top(); proc.stack.pop();
  proc.stack.pop(); // throw away simple arg
  size_t to_skip = countToSkip(seq);
  // skipping instruction
  expect_type<Bytecode>(proc.stack.top())->push("<bc>jmp-nil", to_skip);
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
  c->scar(Object::to_ref(symbols->lookup("<bc>if")));
  c->scdr(proc.stack.top()); proc.stack.pop();
  proc.stack.push(Object::to_ref(c));

  return end;
}

void DbgNameAs::assemble(Process & proc) {
	proc.stack.top(); proc.stack.pop(); // no seq arg
	Object::ref name = proc.stack.top(); proc.stack.pop();
	// set the debug information
	expect_type<Bytecode>(proc.stack.top())->set_name(name);
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
      if (as_a<Symbol*>(car(car(i)))==symbols->lookup("<bc>if")) {
        // must look within the 'if subseq
        n += countConsts(cdr(car(i)));
      } else {
        n++;
      }
    }
  }

  return n;
}

AsOp* Assembler::get_operation(Symbol *s) {
	sym_op_tbl::iterator op = tbl.find(s);
	if (op!=tbl.end()) {
		return op->second;
	} else {
		return NULL;
	}
}

bool AssemblerExecutor::run(Process & proc, size_t & reductions) {
	assembler.go(proc);
	return false;
}

// assemble from a string representation
Object::ref Assembler::inline_assemble(Process & proc, const char *code) {
  std::stringstream code_stream(code);
  read_sequence(proc, code_stream); // leaves seq in the stack
  assembler.go(proc); // take seq from the stack
  Object::ref res = proc.stack.top(); proc.stack.pop();
  return res;
}
