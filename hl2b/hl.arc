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
          (set-axiom var (poly-fn types (fn-axiom parms . body) old))
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
        (<axiom>set ,var (<hl>polymorph ,old ,types (<axiom>lambda ,parms ,@body)))))))
; WARNING! limited 'let only, no destructuring
; for bootstrapping purposes only.  Full destructuring
; support will be added in cs/, but cs/ itself can't
; use it.
(mac <hl>let (var val . rest)
  (if (isnt (type var) 'sym)
      (err "bootstrapping 'let does not support destructuring")
      `((<axiom>lambda (,var)
          ,@rest)
        ,val)))
(mac <hl>with (var-vals . rest)
  (let verify
       (fn (var)
         (if (is (type var) 'sym)
             var
             (err "bootstrapping 'with does not support destructuring")))
    `((<axiom>lambda (,@(map verify:car (pair var-vals)))
        ,@rest)
      ,@(map cadr (pair var-vals)))))
(mac <hl>withs (var-vals . rest)
  (if
    var-vals
      (let (var val . nvars) var-vals
        `(<hl>let ,var ,val
           (<hl>withs ,nvars ,@rest)))
      `((<axiom>lambda () ,@rest))))
(mac <hl>do rest
  `((<axiom>lambda () ,@rest)))
(mac <hl>mac (spec . rest)
  `((<axiom>symeval '<common>call-w/gvl)
    (<axiom>lambda ()
      (<axiom>set ,(car spec)
                  (<axiom>tag '<hl>mac
                    (<axiom>lambda ,(cons '<hl>macro-info (cdr spec))
                      ,@rest))))))

(def <common>call-w/gvl (f) (f))

