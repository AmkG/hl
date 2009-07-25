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

(def <common>call-w/gvl (f) (f))

