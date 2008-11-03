(in-package ll)
(using <arc>v3)
(using <arc>v3-sync)
(using <arc>v3-packages)
(interface v1
  compile
  ll-form
  ll-mac
  ll-add-mac
  ll-add-form
  emit
  hl)

(def emit ss
  (each s ss
    (base-emit s)))

(def base-emit (s)
  (err "emitted unknown type"))
(defm base-emit ((t s string))
  (pr s))
(defm base-emit ((t s sym))
  (pr (mangle s)))
(defm base-emit ((t s int))
  (pr s))
(defm base-emit ((t s bool))
  (if s
      (pr " 1 ")
      (pr " 0 ")))
(defm base-emit ((t s cons))
  (let form (ll-macex s)
    (fn-call-emit form)))

(= ll-mac-tb  (thread-local))
(= ll-form-tb (thread-local))

(def ll-add-mac (nm vl)
  (= ((ll-mac-tb) nm) vl))
(def ll-add-form (nm vl)
  (= ((ll-form-tb) nm) vl))

(def setup ()
  (= (ll-mac-tb) (table))
  (= (ll-form-tb) (table))
  (ll-add-form 'll-mac
    (fn (name parms . body)
      (ll-add-mac name
        (eval `(fn ,parms ,@body)))))
  (ll-add-form 'll-form
    (fn (name parms . body)
      (ll-add-form name
        (eval `(fn ,parms ,@body)))))
  (emit '(ll-form hl rest
           (tostring
             (each e rest
               (eval e))))))

(def teardown ()
  (wipe (ll-mac-tb) (ll-form-tb)))

(def fn-call-emit (s)
  (base-emit s))
(defm fn-call-emit ((t s cons))
  (let (f . args) s
    (aif ((ll-form-tb) f)
         (apply it args)
         (do (emit f "(")
             (when args
               (emit (car args))
               (each a (cdr args)
                 (emit ", " a)))
             (emit ")")))))

(def mangle (s)
  (map
    [if (or (<= #\A _ #\Z) (<= #\a _ #\z) (<= #\0 _ #\9))
        _
        (string "_" (coerce (coerce _ 'int) 'string 16))]
    (string s)))

(def ll-macex (l) l)
(defm ll-macex ((t l cons))
  (let (f . args) l
    (let sub-f (ll-macex f)
      (aif ((ll-mac-tb) f)
           (ll-macex (apply it args))
           l))))

(def read-through (ip)
  (w/uniq eof
    (let cur-cxt (cxt)
      ((afn ()
         (let e (read ip eof)
           (when (isnt e eof)
             (unless (include-check e)
               (let val (cur-cxt e)
                 ; metacommands return t
                 (if (isnt val t)
                     (emit val))))
             (self))))))))

(def include-check (l) nil)
(defm include-check ((t l cons))
  (let (f . rest) l
    (if (is f unpkg!include)
        (do (perform-inclusion (cadr l))
            t))))

(def perform-inclusion (f)
  (err "'include form requires a string"))
(defm perform-inclusion ((t f string))
  (if (is f.0 #\<)
      (emit "#include" f "\n")
      (do (emit "#include\"" f ".cpp\"\n")
          (compile-inner f))))

(def compile (f)
  (dynamic-wind
    setup
    (fn ()
      (compile-inner f))
    teardown))

(def compile-inner (f)
  (w/infile ip f
    (w/outfile op (string f ".cpp")
      (w/stdout op
        (emit "#ifndef " (mangle f) "_CPP\n")
        (emit "#define " (mangle f) "_CPP\n")
        (read-through ip)
        (emit "#endif //" (mangle f) "_CPP\n\n")))))

