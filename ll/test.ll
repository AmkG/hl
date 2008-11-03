
(in-package test)
(using <arc>v3)
(using <ll>v1)
(include "std.ll")
(using <std>v1)
(include "<stdio.h>")

"#define " printf " printf\n"

(def main ()
  (let tmp (print (string "hello"))
    (return 0)))

"int main(void) {\n"
  (return (main)) ";\n"
"}\n"

