/*
 * Read bytecode from a file, run it and print the result (top of the stack) 
 */

#include <iostream>
#include <fstream>

#define DEFINE_GLOBALS 1

#include "all_defines.hpp"
#include "reader.hpp"
#include "executors.hpp"

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl;
  exit(-1);
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

  BytecodeSeq program;
  in >> program;

  bytecode_t *to_run;
  assemble(program, to_run);
  Process p;
  execute(p, 128, 1); // init phase 
  execute(p, 128); // run!

  cout << p.stack.top() << endl;

  return 0;
}
