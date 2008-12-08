/*
 * Read bytecode from a file, run it and print the result (top of the stack) 
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
  bytecode_t *to_run;
  Object::ref program = p.stack.top(); p.stack.pop();
  assemble(program, to_run);
  p.stack.push(Object::to_ref(Closure::NewKClosure(p, to_run, 0))); // entry point
  timeslice = 128;
  execute(p, timeslice, Q); // run!

  cout << p.stack.top() << endl;
  /*
  Object::ref res = p.stack.top();
  if (is_a<int>(res))
    cout << as_a<int>(res) << endl;
  else {
    if (is_a<Symbol*>(res))
      cout << as_a<Symbol*>(res)->getPrintName() << endl;
    else {
      if (is_a<Generic*>(res)) {
        Float *f;
        if ((f = dynamic_cast<Float*>(as_a<Generic*>(res)))!=NULL)
          cout << f->get() << endl;
        else
          cout << "#<unknown type>" << endl;
      }
    }
  }
  */
  return 0;
}
