#include <reader.hpp>

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

// !! missing error checking
std::istream& operator>>(std::istream & in, BytecodeSeq & bc) {
  skip_seps(in);
  if (in.eof())
    return in; // nothing to read

  char c = in.get();
  if (c != bc_start)
    throw ReadError("Unknown character at start of bytecode");

  if (in.eof())
    throw ReadError("EOF");

  std::string name;
  in >> name;

  skip_seps(in);
  c = in.peek();
  if (in.eof())
    throw ReadError("EOF");
  if (c == bc_start) { // subsequence
    BytecodeSeq *sub = new BytecodeSeq;
    in >> (*sub);
    bc.push_back(bytecode(name, sub));
  } else { // simple arg
    size_t val;
    in >> val;
    SimpleArg *sa = new SimpleArg(val);
    skip_seps(in);
    c = in.peek();
    if (c == bc_start) { // simple arg followed by a sequence
      BytecodeSeq *bs = new BytecodeSeq;
      in >> (*bs);
      bc.push_back(bytecode(name, new SimpleArgAndSeq(sa, bs)));
    } else {
      bc.push_back(bytecode(name, new SimpleArg(val)));
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
