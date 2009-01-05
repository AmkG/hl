/*
 * Read bytecode from a file, assemble, disassemble and print
 */
#include "all_defines.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "reader.hpp"
#include "executors.hpp"
#include "symbols.hpp"
#include "types.hpp"

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl << flush;
  exit(1);
}

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
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

  initialize_globals();

  Process p;
  Process* Q;
  size_t timeslice;
  timeslice = 128;
  execute(p, timeslice, Q, 1); // init phase

  read_sequence(p, in);
  assembler.go(p);
  assembler.goBack(p, 0, expect_type<Bytecode>(p.stack.top())->getLen());
  cout << p.stack.top() << endl;

  return 0;
}
