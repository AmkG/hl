#include "types.hpp"

Closure* Closure::NewClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.createVariadic<Closure>(n);
  c->body = body;
  return c;
}
