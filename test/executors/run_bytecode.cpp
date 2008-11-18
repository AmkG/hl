/*
 * Read bytecode from a file, run it and print the result (top of the stack) 
 */
#define DEFINE_GLOBALS
#include "all_defines.hpp"

#include <iostream>
#include <fstream>

#include "reader.hpp"
#include "executors.hpp"
#include "symbols.hpp"

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl << flush;
  exit(1);
}

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
}

/*
 * printing
 */
ostream& operator<<(ostream & o, Object::ref r) {
  if (Object::_is_a<int>(r)) {
    o << Object::to_a_scaled_int(r);
  } else {
    o << "#<unknown type>";
  }
  return o;
}

int main(int argc, char **argv) {
  if (argc!=2) {
    cout << "Wrong number of arguments" << endl;
    return 1;
  }

  ifstream in(argv[1]);
  if (!in) {
    cout << "Can't open file: " << argv[1] << endl;
    return 2;
  }

  boost::scoped_ptr<SymbolsTable> syms(new SymbolsTable);
  symbols.swap(syms);

  BytecodeSeq program;
  try {
    while (!in.eof())
      in >> program;
  } catch (ReadError e) {
    cout << "Reader error: " << e << endl;
    exit(-1);
  }

  bytecode_t *to_run;
  Process p;
  execute(p, 128, 1); // init phase
  assemble(program, to_run);
  p.stack.push(Object::to_ref(Closure::NewClosure(p, to_run, 0))); // entry point
  execute(p, 128); // run!

  cout << p.stack.top() << endl;

  return 0;
}
