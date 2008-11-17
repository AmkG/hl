#include "all_defines.hpp"
#include "reader.hpp"

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

  std::string name;
  in >> name;
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
    size_t val;
    in >> val;
    if (!in)
      throw ReadError("Can't read simple value");
    SimpleArg *sa = new SimpleArg(val);
    skip_seps(in);
    c = in.peek();
    if (c == bc_start) { // simple arg followed by a sequence
      BytecodeSeq *sub = new BytecodeSeq;
      read_bytecodes(in, *sub);
      bc.push_back(bytecode(mnemonic, new SimpleArgAndSeq(sa, sub)));
    } else {
      bc.push_back(bytecode(mnemonic, new SimpleArg(val)));
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
