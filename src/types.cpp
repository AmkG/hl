#include "types.hpp"

Closure* Closure::NewClosure(Heap & h, bytecode_t *body, size_t n) {
  Closure *c = h.create_variadic<Closure>(n);
  c->body = body;
  return c;
}

KClosure* KClosure::NewKClosure(Heap & h, bytecode_t *body, size_t n) {
  KClosure *c = h.create_variadic<KClosure>(n);
  c->body = body;
  return c;
}
