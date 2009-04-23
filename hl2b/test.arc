; basic functionality to test the bytecode generator
; written for hl-arc-f, should be loadable in a hl system
; after changing (using <arc>v3) with (using <hl>v0)

(in-package simple-test)
(using <arc>v3)

; execute tests
; args is a list of of three-elements lists:
; '((test-desc code-to-compile expected-bytecode) ...)
(def do-test (args)
  (with (failed 0 ; number of failed tests
         n-tests (len args))
    (when (is n-tests 0)
      (err "No tests"))
    (each x args 
      (withs (name (car x)
              to-test (cadr x)
              expected (caddr x)
              res (<compiler>compile-to-bytecode to-test))
        (unless (iso res expected)
          (prn "Test " name " " to-test " failed, expected " expected
               " , got " res))
          (set failed (+ failed 1))))
    (prn "Successfully completed " (- n-tests failed) " tests")
    (prn "Failed " failed " tests")
    (prn "Summary:")
    (prn "\tPassed: " (- n-tests failed) "/" n-tests ", " 
         (/ (* 100 (- n-tests failed)) n-tests) "%")
    (prn "\tFailed: " failed "/" n-tests ", "
         (/ (* 100 failed) n-tests) "%")))

(set all-tests* nil)

