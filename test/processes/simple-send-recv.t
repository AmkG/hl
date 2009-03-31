(<bc>self-pid)
(global-set main-pid)
(k-closure 0
  (check-vars 1)
  (global main-pid)
  (int 42)
  (<bc>send)
  (halt))
(<bc>spawn)
(k-closure 0
  (local 1)
  (halt))
(<bc>recv)
