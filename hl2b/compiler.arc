; front end to the bytecode compiler
; loads all the files and defines an entry function

; define the <hl> package and import functions and macros needed 
; by the compiler from <arc>v3
; TODO: move in a file conditionally loaded only when bootstrapping
;       using hl-arc-f
(in-package hl)
(using <arc>v3)
; Should really use <hl>def, not just def, because def in this
; context will be <arc>def.  Same with <hl>type etc.
; Please see http://hl.webhop.net/item?id=103 ; I recommended
; using 3 sections, and specified which ones should specify
; specific packages, and which ones just use <arc> packages.
(interface v0 def mac if and no or each load string unpkg prn = w/infile 
              let with withs fn afn require w/uniq coerce + push listtab
              givens make-br-fn ++ acons ssyntax isnt caris _ in type isa
              alist err is car cons cdr map orf awhen do trav+ when ontable
              list aif zap self makeproper rfn pos complement dotted len on
              index copy mappend cadr cddr len> it some assert given case
              >= > keep idfn rev - mem max mapeach
              ; types
              int num char table string sym bool)

(set <hl>unpkg <arc>unpkg)
(set <hl>ssyntax <arc>ssyntax)
(set <hl>pos <arc>pos)

; the compiler package
(in-package compiler)
(using <hl>v0)

(interface v0 compile)

; this var tells if we are bootstrapping the compiler or not
(set bootstrap *t)

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

(when (> (len <arc>argv) 1)
  ; called from the command line
  ; compile and run the program
  (<arc>w/infile in (<arc>argv 1)
    (let prog `((<axiom>lambda () ,@(<arc>readall in)))
      (<arc>w/outfile tmp "/tmp/hl-tmp"
        ; not the best thing to do, pipe-to would be better if we had it
        ; put the default continuation
        (<arc>write '(<bc>k-closure 0 
                       (<bc>check-vars 2) 
                       (<bc>local 1) 
                       (<bc>halt))
                    tmp)
        (each expr (compile-to-bytecode prog)
          (prn expr)
          (<arc>write expr tmp)))))
  ; run the bytecode
  (<arc>system:string "../src/hl --bc /tmp/hl-tmp"))
