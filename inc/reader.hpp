#ifndef READER_H
#define READER_H

#include <istream>
#include <string>
#include <vector>

#include "all_defines.hpp"
#include "symbols.hpp"

/*
 * Intermediate bytecode representation constructed by the reader
 * The execution engine should assemble it in a representation suitable
 * for execution.
 */

// the argument of a bytecode
class BytecodeArg { public: virtual ~BytecodeArg() {}};

/*
 * a simple argument is an unsigned integer
 * it could represent an int, a scaled int, an offset or a character
 */
class SimpleArg : public BytecodeArg {
private:
  size_t val;
public:
  SimpleArg(size_t v) : val(v) {}
  size_t getVal() { return val; }
};

// a single bytecode
typedef std::pair<Symbol*, BytecodeArg*> bytecode;

/*
 * bytecode sequence
 * owns all the bytecode args and frees them
 */
class BytecodeSeq : public std::vector<bytecode>, public BytecodeArg {
public:
  ~BytecodeSeq();
};

/*
 * Argument of bytecode that takes a simple argument and a sequence
 */
class SimpleArgAndSeq : public BytecodeArg {
private:
  BytecodeSeq *seq;
  SimpleArg *sa;
public:
  SimpleArgAndSeq(SimpleArg *s, BytecodeSeq *bs) : sa(s), seq(bs) {}
  ~SimpleArgAndSeq() {
    delete seq;
    delete sa;
  }
  SimpleArg* getSimple() { return sa; }
  BytecodeSeq* getSeq() { return seq; }
};

// exception thrown by the reader
class ReadError : public std::string {
public:
  ReadError(const char *str) : std::string(str) {}
};

/*
 * Bytecode reader
 * extends bc with a bytecode read from in
 */
std::istream& operator>>(std::istream & in, BytecodeSeq & bc);

#endif // READER_H
