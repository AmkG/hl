#include "all_defines.hpp"

#include <reader.hpp>
#include <iostream>
#include <fstream>

using namespace std;

void throw_HlError(const char *str) {
  cout << "Error: " << str << endl << flush;
  exit(1);
}

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
}

void printSimpleArg(SimpleArg *sa) {
  if (is_a<int>(sa->getVal()))
    cout << Object::to_a_scaled_int(sa->getVal());
  else {
    if (is_a<Symbol*>(sa->getVal()))
      cout << as_a<Symbol*>(sa->getVal())->getPrintName();
    else
      cout << "#<unknown argument type>";
  }
}

void printBs(BytecodeSeq & bs) {
  for (BytecodeSeq::iterator i = bs.begin(); i!=bs.end(); i++) {
    cout << "(" << i->first->getPrintName();
    if (i->second!=NULL) {
      cout << " ";
      BytecodeArg *a = i->second;
      SimpleArg *sa;
      if (sa = dynamic_cast<SimpleArg*>(a))
        printSimpleArg(sa);
      BytecodeSeq *seq;
      if (seq = dynamic_cast<BytecodeSeq*>(a)) {
        printBs(*seq);
      }
      SimpleArgAndSeq *args;
      if (args = dynamic_cast<SimpleArgAndSeq*>(a)) {
        printSimpleArg(args->getSimple());
        cout << " ";
        printBs(*(args->getSeq()));
      }
    }
    cout << ")";
  }
}

int main(int argc, char **argv) {
  std::ifstream in(argv[1]);

  boost::scoped_ptr<SymbolsTable> syms(new SymbolsTable);
  symbols.swap(syms);

  BytecodeSeq bs;
  try {
    in >> bs;
    printBs(bs);
  } 
  catch (ReadError re) {
    cout << re;
  }
  return 0;
}
