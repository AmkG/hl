; basic functionality to test the bytecode generator
; written for hl-arc-f, should be loadable in a hl system
; after changing (using <arc>v3) with (using <hl>v0)

(in-package simple-test)
(using <arc>v3)
(interface v0 test run-tests)

; execute tests and report results
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

; list of registered tests
(set all-tests* nil)

; add a test (last added, last executed)
(def register-test (test)
  (set all-tests* (append all-tests* (list test))))

; add a set of tests with the same name
; usage:
;   (test "Description"
;     input-program1 expected-result1
;     input-program2 expected-result2
;     ...)
(mac test (name . tests)
  (unless (is 0 (mod (len tests) 2))
    (err "Unpaired test/result"))
  `(do
     ,@(map (fn (tst)
              `(register-test (cons name ,tst)))
            (tuples ,tests 2))))

; run all the registered tests
(def run-tests ()
  (do-tests all-tests*))
