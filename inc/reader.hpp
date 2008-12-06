#ifndef READER_H
#define READER_H

#include <istream>
#include <string>
#include "objects.hpp"
#include "symbols.hpp"

class Process;
// leaves read bytecode on the stack
void read_bytecode(Process & proc, std::istream & in);
void read_sequence(Process & proc, std::istream & in);

// simple printer for debugging
std::ostream& operator<<(std::ostream & out, Object::ref obj);

#endif // READER_H
