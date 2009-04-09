#include "all_defines.hpp"
#include "symbols.hpp"
#include "generics.hpp"
#include <string>

class Process;
void throw_HlError(char const*);

void Generic::call(Process & proc, size_t & reductions){
	/*TODO: in the future lookup the object type in a 
	  user defined call table*/
	std::string t = as_a<Symbol*>(type())->getPrintName();
	throw_HlError(("object of type " + t + " isn't callable").c_str());
}
