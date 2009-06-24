#include "all_defines.hpp"
#include "reader.hpp"
#include "types.hpp"
#include "processes.hpp"
#include "symbols.hpp"
#include "executors.hpp"

#include <iostream>
#include <sstream>

static const char bc_start = '(';
static const char bc_end = ')';
static const char *separators = " \n\t";

void skip_seps(std::istream & in) {
  while (!in.eof() && strchr(separators, in.peek())) 
    in.get();
}

// read a sequence of bytecodes in a list left on the stack
void read_sequence(Process & proc, std::istream & in) {
  char c;
  proc.stack.push(Object::nil()); // head
  proc.stack.push(Object::nil()); // tail
  while (1) {
    skip_seps(in);
    c = in.peek();
    if (!in.eof() && c == bc_start) {
      read_bytecode(proc, in);
      Object::ref c2 = Object::to_ref(proc.create<Cons>());
      if (proc.stack.top(3)==Object::nil()) // test the head
        proc.stack.top(3) = c2; // new head
      else
        scdr(proc.stack.top(2), c2); // cdr of the tail
      scar(c2, proc.stack.top()); proc.stack.pop();
      proc.stack.top() = c2; // new tail
    }
    else 
      break;
  }
  proc.stack.pop(); // remove tail and leave head
}

// read a string until a separator character is found
std::string read_upto(std::istream & in) {
  static const char *no_sym = " \t\n()";
  std::string res;
  char c;
  c = in.peek();
  while (!in.eof() && strchr(no_sym, c)==NULL) {
    res += in.get();
    c = in.peek();
  }
  if (res=="") 
    in.setstate(std::ios_base::badbit);
  return res;
}

// read a number or a symbol, leave it on the stack
void read_atom(Process & proc, std::istream & in) {
  std::string res = read_upto(in);
  std::stringstream s(res);
  if (res.find('.')!=-1) { // try to parse a float
    double f;
    s >> f;
    if (!s) // it's a symbol
      proc.stack.push(Object::to_ref(symbols->lookup(res)));
    else
      proc.stack.push(Object::to_ref(Float::mk(proc, f)));
  }
  else { // try to parse an int
    int i;
    s >> i; 
    if (!s) // it's a symbol
      proc.stack.push(Object::to_ref(symbols->lookup(res)));   
    else
      proc.stack.push(Object::to_ref(i));
  }
}

void read_bytecode(Process & proc, std::istream & in) {
  // !! the created object must stay on the stack for correct
  // !! collection
  proc.stack.push(Object::to_ref(proc.create<Cons>()));
  if (!in)
    throw_HlError("Input stream is invalid");

  skip_seps(in);
  if (in.eof())
    return; // nothing to read

  char c = in.get();
  if (c != bc_start) {
    std::string err = "Unknown character at start of bytecode";
    err += " ";
    err += c;
    throw_HlError(err.c_str());
  }

  if (in.eof())
    throw_HlError("EOF");

  std::string name = read_upto(in);
  if (!in)
    throw_HlError("Can't read bytecode mnemonic");
  Symbol *mnemonic = symbols->lookup(name);
  scar(proc.stack.top(), Object::to_ref(mnemonic));

  skip_seps(in);
  c = in.peek();
  if (in.eof())
    throw_HlError("EOF");
  if (c == bc_end) { // single mnemonic
    in.get();
    return;
  }
  if (c == bc_start) { // subsequence
    read_sequence(proc, in);
    Object::ref sub = proc.stack.top(); proc.stack.pop();
    scdr(proc.stack.top(), sub);
  } else { // simple arg
    Cons *c2 = proc.create<Cons>();
    scdr(proc.stack.top(), Object::to_ref(c2));
    read_atom(proc, in);
    if (!in)
      throw_HlError("Can't read simple value");
    skip_seps(in);
    c = in.peek();
    if (c == bc_start) { // simple arg followed by a sequence
      read_sequence(proc, in);
      Object::ref sub = proc.stack.top(); proc.stack.pop();
      Object::ref atom = proc.stack.top(); proc.stack.pop();
      scar(cdr(proc.stack.top()), atom);
      scdr(cdr(proc.stack.top()), sub);
    } else {
      Object::ref atom = proc.stack.top(); proc.stack.pop();
      scar(cdr(proc.stack.top()), atom);
    }
  }

  skip_seps(in);
  c = in.get();
  if (in.eof())
    throw_HlError("EOF");
  if (c != bc_end)
    throw_HlError("Spurious contents at end of bytecode");
}

std::ostream& operator<<(std::ostream & out, Object::ref obj) {
  if (obj==Object::nil()) {
    out << "nil";
  } else if (obj==Object::t()) {
    out << "t";
  }else if (is_a<int>(obj)) {
    out << as_a<int>(obj);
  } else if (is_a<Symbol*>(obj)) {
    out << as_a<Symbol*>(obj)->getPrintName();
  } else if (is_a<Generic*>(obj)) {
    Generic *g = as_a<Generic*>(obj);
    Float *f;
    Cons *c;
		Closure *l;
		HlString *str;
    if (f = dynamic_cast<Float*>(g)) {
      out.setf(std::ios::showpoint);
      out << f->get();
      out.unsetf(std::ios::showpoint);
    } else if (c = dynamic_cast<Cons*>(g)) {
      out << "(" << c->car();
      Object::ref r = c->cdr();
      Cons *c2;
      while (is_a<Generic*>(r) && 
             (c2 = dynamic_cast<Cons*>(as_a<Generic*>(r)))) {
        out << " " << c2->car();
        r = c2->cdr();
      }
      if (r==Object::nil())
        out << ")";
      else
        out << " . " << r << ")";
    } else if (l = dynamic_cast<Closure*>(g)) {
			out << "#<fn>";
		} else if (str = maybe_type<HlString>(obj)) {
			out << '"' << str->to_cpp_string() << '"';
		} else {
	    std::string name = as_a<Symbol*>(type(obj))->getPrintName();
	    out << "#<" << name << ">";
    }
  }
  else {
    out << "#<??>";
  }
  
  return out;
}
