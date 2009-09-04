(in-package hl)
(using <arc>v3)

(mac <hl>def (spec . rest)
  (if
    (acons spec)
      `((<axiom>symeval '<common>call-w/gvl)
         (<axiom>lambda ()
           (<axiom>set ,(car spec)
             ; should really be <hl>fn, but for now we use
             ; <axiom>lambda
             (<axiom>lambda ,(cdr spec)
               ,@rest))))
      `((<axiom>symeval '<common>call-w/gvl)
         (<axiom>lambda ()
           (<axiom>set ,spec
             ,(car rest))))))
(mac <hl>defm rest
  (withs (f  (rep <arc>defm)
          rv (apply f rest))
    ; find the <arc>polymorph and replace with
    ; <hl>polymorph
    ; NOTE! differences in <arc>polymorph and
    ; <hl>polymorph:
    ;   (<arc>polymorph signature generic specific)
    ;   (<hl>polymorph generic signature specific)
    ; - the change in order was done since we might
    ;   want to overload how overloading of particular
    ;   objects as generics are done
    ((afn (v)
       (when (acons v)
         (when (caris v '<arc>polymorph)
           (= (car v) '<hl>polymorph)
           (with (first-par  (cdr  v)
                  second-par (cddr v))
             (= (cdr v)          second-par)
             (= (cdr first-par)  (cdr second-par))
             (= (cdr second-par) first-par)))
         (self (car v))
         (self (cdr v))))
     rv)
    ; wrap in w/gvl
    `((<axiom>symeval '<common>call-w/gvl)
      (<axiom>lambda ()
        ,rv))))

(def <common>call-w/gvl (f) (f))

