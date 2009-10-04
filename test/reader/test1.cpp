#include "all_defines.hpp"
#include "types.hpp"
#include "symbols.hpp"
#include "reader.hpp"
#include "executors.hpp"

#include <iostream>
#include <fstream>

using namespace std;

void throw_OverBrokenHeart(Generic*) {
  throw_HlError("overbrokenheart");
}

bool GoNextBoot::run(Process& proc, size_t& reductions) {
	throw_HlError("go-next-boot not implemented during tests");
}

int main(int argc, char **argv) {
  std::ifstream in(argv[1]);

  boost::scoped_ptr<SymbolsTable> syms(new SymbolsTable);
  symbols.swap(syms);

  Process proc;
  read_bytecode(proc, in);
  cout << proc.stack.top();
  return 0;
}
