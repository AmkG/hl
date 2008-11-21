#include "all_defines.hpp"
#include "types.hpp"

Closure* Closure::NewClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = body;
  c->nonreusable = true;
  c->kontinuation = false;
  return c;
}

Closure* Closure::NewKClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.lifo_create_variadic<Closure>(n);
  c->body = body;
  c->nonreusable = false;
  c->kontinuation = true;
  return c;
}
