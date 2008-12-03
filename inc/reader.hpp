#ifndef READER_H
#define READER_H

#include <istream>
#include <string>
#include <vector>

#include "symbols.hpp"

/*
 * Intermediate bytecode representation constructed by the reader
 * The execution engine should assemble it in a representation suitable
 * for execution.
 */

// the argument of a bytecode
class BytecodeArg { public: virtual ~BytecodeArg() {}};

/*
 * a simple argument is a value that can fit in a pointer
 * it could be an int, a scaled int, an offset, a character, a pointer
 * to a Symbol, etc.
 */
class SimpleArg : public BytecodeArg {
private:
  Object::ref val;
public:
  SimpleArg() {}
  template <class T>
  void setVal(T v) { val = Object::to_ref(v); }
  Object::ref getVal() { return val; }
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
  SimpleArg *sa;
  BytecodeSeq *seq;
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
