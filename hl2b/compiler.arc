; front end to the bytecode compiler
; loads all the files and defines an entry function

; define the <hl> package and import functions and macros needed 
; by the compiler from <arc>v3
; TODO: move in a file conditionally loaded only when bootstrapping
;       using hl-arc-f
(in-package hl)
(using <arc>v3)
(interface v0 def mac if and no or each load string unpkg prn = w/infile 
              let with withs fn afn require w/uniq coerce + push listtab
              givens make-br-fn ++ acons ssyntax isnt caris _ in type isa
              alist err is car cons cdr maporf awhen do trav+ when ontable
              list aif zap self makeproper rfn pos complement dotted len on
              index
              ; types
              int table string sym)

(set <hl>unpkg <arc>unpkg)
(set <hl>ssyntax <arc>ssyntax)
(set <hl>pos <arc>pos)

; the compiler package
(in-package compiler)
(using <hl>v0)

(interface v0 compile)

; strictly, "compiled-and-executed-files"
(set files* '("structs.hl" "bytecodegen.hl" 
              "quote-lift.hl" "utils.hl" "closures.hl" "cps.hl"
              "sharedvars.hl"  "xe.hl"))
; need also *another* set of files for "compiled-only-but-not-executed-files"

; load all the files
(each f files*
  (load f))

; entry point
; return a 0 arg function that executes expr
;(def compile (expr)
;  (<axiom>assemble (compile-to-bytecode expr)))
; above doesn't actually make sense from within
; Arc-F, because Arc-F doesn't support <axiom>assemble.
; Maybe should be in the "compiled-but-not-executed"
; set of files
