#include <reader.hpp>
#include <iostream>
#include <fstream>

using std::cout;

void printBs(BytecodeSeq & bs) {
  for (BytecodeSeq::iterator i = bs.begin(); i!=bs.end(); i++) {
    cout << "(" << i->first << " ";
    BytecodeArg *a = i->second;
    SimpleArg *sa;
    if (sa = dynamic_cast<SimpleArg*>(a)) {
      cout << sa->getVal();
    }
    BytecodeSeq *seq;
    if (seq = dynamic_cast<BytecodeSeq*>(a)) {
      printBs(*seq);
    }
    SimpleArgAndSeq *args;
    if (args = dynamic_cast<SimpleArgAndSeq*>(a)) {
      cout << args->getSimple()->getVal() << " ";
      printBs(*(args->getSeq()));
    }
    cout << ")";
  }
}

int main(int argc, char **argv) {
  std::ifstream in(argv[1]);
  BytecodeSeq bs;
  try {
    in >> bs;
  } 
  catch (ReadError re) {
    cout << re;
  }
  printBs(bs);
  return 0;
}
