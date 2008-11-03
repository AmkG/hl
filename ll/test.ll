
(in-package test)
(using <arc>v3)
(using <ll>v1)
(include "std.ll")
(using <std>v1)

(def main ()
  (return 0))

"int main(void) {\n"
  (return (main)) ";\n"
"}\n"

