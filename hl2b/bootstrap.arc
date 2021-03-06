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
              make-br-fn ++ acons ssyntax isnt caris _ in type isa
              alist err is car cons cdr map orf awhen do trav+ when ontable
              list aif zap self rfn pos complement dotted len on
              index copy mappend cadr cddr len> it some assert case
              >= > keep idfn rev - mem max mapeach atom bound rep apply
              ; types
              int num char table string sym bool)

(set <hl>unpkg <arc>unpkg)
(set <hl>ssyntax <arc>ssyntax)
(set <hl>pos <arc>pos)

; fix differences between the host and the target
; this is actually a superset, i.e. they do *at least* what their hl
; counterpart do
(mac <axiom>lambda (args . body)
  `(fn ,args ,@body))

(set <axiom>tag <arc>annotate)
(set <axiom>cons <arc>cons)
(set <axiom>i- -)
(set <axiom>i+ +)
(set <axiom>i* *)
(set <axiom>i< <)
(set <axiom>is is)
(set <axiom>cdr cdr)
(set <axiom>car car)

; the compiler package
(in-package compiler)
(using <hl>v0)

(set <compiler>ppr-sexp prn)

(interface v0 compile)

; this var tells if we are bootstrapping the compiler or not
(set bootstrap* t)
; tells if the host should eval the code before compiling it
; eval is needed to get macros to work while bootstrapping
; but the host doesn't support some things (mainly async I/O & processes)
(set stop-host-eval* nil)

; strictly, "compiled-and-executed-files"
(set files* '("structs.hl" "bytecodegen.hl" 
              "quote-lift.hl" "utils.hl" "closures.hl" "cps.hl"
              "sharedvars.hl"  "xe.hl" "macex.hl"
              "hl.arc"))
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

(using <read-cs-dir>v1)

; usage:
;  arc bootstrap.arc
;    - compiles the entire cs/ directory as a
;      single file in /tmp/hl-tmp
;    - provides a halting continuation
;  arc bootstrap.arc file
;    - compiles the specified file into
;      /tmp/hl-tmp
;    - provides a halting continuation
;  arc bootstrap.arc file --next
;    - compiles the specified file into
;      /tmp/hl-tmp
;    - provides a next-boot-file continuation

(= exprs* (if (> (len <arc>argv) 1)
            (read-cs-dir (list (cadr <arc>argv)))
            (read-cs-dir)))
(= use-next-boot-cont*
   (is (cadr:cdr <arc>argv) "--next"))
(= stop-host-eval* t)

; if --next flag is given, trap errors
;   so that makefile aborts
; maintainer has to hack stuff just to
;   find out what the error is.
; stupid misdesign of error trapping in
;   arc >.<
((if use-next-boot-cont*
     <arc>on-err
     (fn (_ f) (f)))
  (fn (e)
    (<arc>quit 1))
  (fn ()
    (withs (exprs exprs*
            prog `((<axiom>lambda () ,@exprs)))
      (<arc>w/outfile tmp "/tmp/hl-tmp"
        ; not the best thing to do, pipe-to would be better if we had it
        ; put the default continuation
        (<arc>write `(<bc>k-closure 0
                       (<bc>debug-name final-return)
                       (<bc>check-vars 2) 
                       (<bc>local 1) 
                       ,(if use-next-boot-cont*
                            '(<bc>do-executor <impl>go-next-boot)
                            '(<bc>halt)))
                    tmp)
        (prn "pre-eval")
        (each expr exprs
          ; for each expression, we compile it & then eval it
          ; when bootstrapping evaluation is done in the host
          ; and the compiled result is not executed
          ; WARNING: the *whole* file is evaluated before compilation
          ; takes place. This means that if a global var is redefined
          ; by the compiled code, during compilation only the last definition
          ; will be available. This shouldn't be a problem in practice.
          (if
            (and (isa expr 'sym) (is (unpkg expr) (unpkg '>stop-host-eval)))
              (set stop-host-eval* t)
            (and (isa expr 'sym) (is (unpkg expr) (unpkg '>start-host-eval)))
              (set stop-host-eval* nil)
            (and bootstrap* (no stop-host-eval*))
              (<arc>eval (<compiler>macex expr))))
        (prn "compile")
        (each bc (compile-to-bytecode prog)
          (<arc>write bc tmp)
          (<arc>writec #\newline tmp))))))
