#include "all_defines.hpp"
#include "reader.hpp"
#include "types.hpp"

#include <iostream>
#include <sstream>

static const char bc_start = '(';
static const char bc_end = ')';
static const char *separators = " \n\t";

BytecodeSeq::~BytecodeSeq() {
  for (BytecodeSeq::iterator it = begin(); it!=end(); it++) {
    delete (it->second);
  }
}

void skip_seps(std::istream & in) {
  while (!in.eof() && strchr(separators, in.peek())) 
    in.get();
}

// read a sequence of bytecodes
void read_bytecodes(std::istream & in, BytecodeSeq & bc) {
  char c;
  while (1) {
    skip_seps(in);
    c = in.peek();
    if (!in.eof() && c == bc_start) {
      in >> bc;
    }
    else break;
  }
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

// read a number or a symbol
std::istream& operator>>(std::istream & in, SimpleArg & sa) {
  std::string res = read_upto(in);
  std::stringstream s(res);
  if (res.find('.')!=res.length()) { // try to parse a float
    double f;
    s >> f;
    // we don't have a Heap right now, and anyway we don't want to 
    // scan an entire bytecode sequence on every GC to look for float
    // literals. They will be few, anyway.
    sa.setVal(Float::mkEternal(f));
  }
  else { // try to parse an int
    int i;
    s >> i; 
    if (!s) // it's a symbol
      sa.setVal(symbols->lookup(res));
    else
      sa.setVal(i);
  }

  return in;
}

std::istream& operator>>(std::istream & in, BytecodeSeq & bc) {
  if (!in)
    throw ReadError("Input stream is invalid");

  skip_seps(in);
  if (in.eof())
    return in; // nothing to read

  char c = in.get();
  if (c != bc_start) {
    std::string err = "Unknown character at start of bytecode";
    err += " ";
    err += c;
    throw ReadError(err.c_str());
  }

  if (in.eof())
    throw ReadError("EOF");

  std::string name = read_upto(in);
  if (!in)
    throw ReadError("Can't read bytecode mnemonic");
  Symbol *mnemonic = symbols->lookup(name);

  skip_seps(in);
  c = in.peek();
  if (in.eof())
    throw ReadError("EOF");
  if (c == bc_end) { // single mnemonic
    in.get();
    bc.push_back(bytecode(mnemonic, NULL));
    return in;
  }
  if (c == bc_start) { // subsequence
    BytecodeSeq *sub = new BytecodeSeq;
    read_bytecodes(in, *sub);
    bc.push_back(bytecode(mnemonic, sub));
  } else { // simple arg
    SimpleArg *sa = new SimpleArg();
    in >> (*sa);
    if (!in) {
      delete sa;
      throw ReadError("Can't read simple value");
    }
    skip_seps(in);
    c = in.peek();
    if (c == bc_start) { // simple arg followed by a sequence
      BytecodeSeq *sub = new BytecodeSeq;
      read_bytecodes(in, *sub);
      bc.push_back(bytecode(mnemonic, new SimpleArgAndSeq(sa, sub)));
    } else {
      bc.push_back(bytecode(mnemonic, sa));
    }
  }

  skip_seps(in);
  c = in.get();
  if (in.eof())
    throw ReadError("EOF");
  if (c != bc_end)
    throw ReadError("Spurious contents at end of bytecode");
  
  return in;
}
