#ifndef READER_H
#define READER_H

#include <istream>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

/*
 * Intermediate bytecode representation constructed by the reader
 * The execution engine should assemble it in a representation suitable
 * for execution.
 */

// the argument of a bytecode
class BytecodeArg {};

// a single bytecode
typedef std::pair<std::string, BytecodeArg*> bytecode;

// bytecode sequence
// owns all the bytecode args and frees them
class BytecodeSeq : public std::vector<bytecode>, public BytecodeArg {
public:
  ~BytecodeSeq();
};

typedef boost::shared_ptr<BytecodeSeq> bytecodeseq_ptr;

/*
 * Bytecode reader
 */
class Reader {
private:
  std::istream *in;
  bytecodeseq_ptr bc;
public:
  Reader(std::istream *i) : in(i), bc(new BytecodeSeq()) {}
  ~Reader() { }

  // read bytecodes until EOF
  void readAll();

  // read the next bytecode
  void readOne();

  // return the current bytecode seuence
  bytecodeseq_ptr getBytecode() { return bc; }
};

#endif // READER_H
