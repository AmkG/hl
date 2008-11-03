(in-package std)
(using <arc>v3)
(using <ll>v1)
(interface v1
  + - * /
  bit-and bit-or bit-xor bit-not
  bit-shift bit-rshift
  and or not no
  is isnt
  <= >= < >
  = set
  malloc free realloc
  const var def
  return)
(include "<stdint.h>")
(include "<cstring>")
(include "<stdlib.h>")

;-----------------------------------------------------------------------------
;Expression operations
;-----------------------------------------------------------------------------

(hl
  (def math (oper rest)
    (if (and rest (cdr rest))
        (do (emit "(" (car rest) ")")
            (each e (cdr rest)
              (emit " " oper " (" e ")")))
        (err oper " requires at least two parameters")))
  (def bi-math (oper x y)
    (emit "(" x ") " oper " (" y ")")))

(ll-form + rest		(math "+" rest))
(ll-form - rest
  (if (no:cdr rest)
      (emit "(-(" (car rest) "))")
      (math "-" rest)))
(ll-form * rest		(math "*" rest))
(ll-form / rest		(math "/" rest))

(ll-form bit-and rest	(math "&" rest))
(ll-form bit-or rest	(math "|" rest))
(ll-form bit-xor rest	(math "^" rest))
(ll-form bit-not (x)
  (emit "~(" x ")"))
(ll-form bit-shift (x y)	(bi-math "<<" x y))
(ll-form bit-rshift (x y)	(bi-math ">>" x y))

(ll-form and rest	(math "&&" rest))
(ll-form or rest	(math "||" rest))
(ll-form not (x)
  (emit "!(" x ")"))
(ll-form no (x)
  (emit "!(" x ")"))

(ll-form is (x y)	(bi-math "==" x y))
(ll-form isnt (x y)	(bi-math "!=" x y))

(ll-form < (x y)	(bi-math "<" x y))
(ll-form > (x y)	(bi-math ">" x y))
(ll-form <= (x y)	(bi-math "<=" x y))
(ll-form >= (x y)	(bi-math ">=" x y))

(ll-form = (x y)	(bi-math "=" x y))
(ll-form set (x y)	(bi-math "=" x y))

;-----------------------------------------------------------------------------
;Memory allocation
;-----------------------------------------------------------------------------

(ll-form malloc (sz)
  (emit "(intptr_t)malloc((size_t) " sz ")"))
(ll-form free (ptr)
  (emit "free((void*) " ptr ")"))
(ll-form realloc (ptr sz)
  (emit "(intptr_t)realloc((void*) " ptr ", (size_t) " sz ")"))

;-----------------------------------------------------------------------------
;Top-level forms
;-----------------------------------------------------------------------------

(ll-form const (var val)
  (emit "const intptr_t " var " = " val ";\n"))
(ll-form var (var (o val))
  (emit "intptr_t var " var " = " val ";\n"))

(ll-form def (name parms . body)
  (emit "\nintptr_t " name "(")
  (if parms
      (do (emit "intpr_t " (car parms))
          (each p (cdr parms)
            (emit ", intptr_t " p)))
      (emit "void"))
  (emit ") {\n")
  (each b body
    (emit b ";\n"))
  (emit "}\n"))

;-----------------------------------------------------------------------------
;Statements
;-----------------------------------------------------------------------------

(ll-form return (x)
  (emit "return " x))

