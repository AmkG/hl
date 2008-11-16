#include "types.hpp"

Closure* Closure::NewClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = body;
  return c;
}

Closure* Closure::NewKClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = body;
  c->nonreusable = false;
  return c;
}
