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
(mac <hl>defm (sig . rest)
  (withs (f  (rep <arc>defm)
          rv (apply f (car sig) (cdr sig) rest)
          (set-axiom var (poly-fn types old new))
             rv)
    ; NOTE! differences in <arc>polymorph and
    ; <hl>polymorph:
    ;   (<arc>polymorph signature generic specific)
    ;   (<hl>polymorph generic signature specific)
    ; - the change in order was done since we might
    ;   want to overload how overloading of particular
    ;   objects as generics are done
    ; wrap in w/gvl
    `((<axiom>symeval '<common>call-w/gvl)
      (<axiom>lambda ()
        (<axiom>set ,var (<hl>polymorph ,old ,types ,new))))))

(def <common>call-w/gvl (f) (f))

