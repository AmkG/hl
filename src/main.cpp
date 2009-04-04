/*
 * Example hl driver -- execute bytecode
 */
#include "all_defines.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "reader.hpp"
#include "executors.hpp"
#include "symbols.hpp"
#include "types.hpp"
#include "workers.hpp"

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl << flush;
  exit(1);
}

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
}

void usage() {
  cout << "hl file1 [file2 ...]" << endl;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage();
    return 1;
  }

  initialize_globals();

  Process *p;
  Process *Q;
  size_t timeslice;
  timeslice = 128;

  p = new Process();
  while(execute(*p, timeslice, Q, 1) == process_running); // init phase
  delete p;

  for (int i = 1; i < argc; i++) {
    ifstream in(argv[i]);
    if (!in) {
      cerr << "Can't open file: " << argv[i] << endl;
      return 2;
    }

    p = new Process();
    read_sequence(*p, in);
    assembler.go(*p);
    Closure *k = Closure::NewKClosure(*p, 0);
    k->codereset(p->stack.top()); p->stack.pop();
    p->stack.push(Object::to_ref(k)); // entry point
    // process will be deleted by workers
    AllWorkers &w = AllWorkers::getInstance();
    w.initiate(3, p);
    cout << p->stack.top() << endl; // print result
  }

  return 0;
}
