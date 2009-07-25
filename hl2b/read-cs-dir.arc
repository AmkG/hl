
(in-package read-cs-dir)
(using <arc>v3)
(using <arc>v3-packages)
(interface <read-cs-dir>v1
  read-cs-dir)

(mac restartable body
  `(let restart
        (symeval!ccc
          (fn (k)
            (afn () (k self))))
     ,@body))

(def list-cs-files ()
  (withs (tmp   (tostring:system "sh -c \"echo cs/*.hl\"")
          i     0
          ln    (len tmp)
          stop? nil)
    (w/collect
      (restartable
        (collect
          (w/collect-on ""
            (breakable:restartable:let c tmp.i
              ++.i
              (if
                (is c #\space)
                  (break t)
                (whitec c)
                  () ; do nothing
                ; else
                  (collect c))
              (if (>= i ln)
                  (= stop? t)
                  (restart)))))
        (unless stop? (restart))))))

; returns a list of all expressions
(def read-cs-dir ()
  (w/collect:breakable:each f (sort < (list-cs-files))
    (with (context (cxt)
           eof     (uniq)
           e       ())
      (w/infile p f
        (while (isnt (= e (read p eof)) eof)
          (if (is e unpkg!>stop-compiling)
              (break t))
          (collect (context e)))))))

