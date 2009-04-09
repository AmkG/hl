#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

#include<cstring>
#include<boost/shared_ptr.hpp>

#include"objects.hpp"
#include"specializeds.hpp"
#include"heaps.hpp"
#include"processes.hpp"

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

/*Usage:
Cons* cp = expect_type<Cons>(proc.stack().top(),
		"Your mom expects a Cons cell on top"
);
*/
template<class T>
static inline T* expect_type(Object::ref x, char const* error = "type error") {
	if(!is_a<Generic*>(x)) throw_HlError(error);
	T* tmp = dynamic_cast<T*>(as_a<Generic*>(x));
	if(!tmp) throw_HlError(error);
	return tmp;
}

/*Usage:
// your mom knows it's a Cons on top
Cons* cp = known_type<Cons>(proc.stack().top());
*/
template<class T>
static inline T* known_type(Object::ref x) {
	return static_cast<T*>(as_a<Generic*>(x));
}

/*
Cons* cp = maybe_type<Cons>*(proc.stack().top());
if(cp) {
	your_mom_does_a_cons(cp);
} else {
	your_mom_does_something_else();
}
*/
template<class T>
static inline T* maybe_type(Object::ref x) {
	if(!is_a<Generic*>(x)) return NULL;
	return dynamic_cast<T*>(as_a<Generic*>(x));
}

/*-----------------------------------------------------------------------------
Cons cell
-----------------------------------------------------------------------------*/

class Cons : public GenericDerived<Cons> {
private:
  
  Object::ref car_ref;
  Object::ref cdr_ref;

public:

  inline Object::ref& car() { return car_ref; }
  inline Object::ref& cdr() { return cdr_ref; }
  inline Object::ref scar(Object::ref x) { return car_ref = x; }
  inline Object::ref scdr(Object::ref x) { return cdr_ref = x; }

  Cons() : car_ref(Object::nil()), cdr_ref(Object::nil()) {}
  /*
   * Note that we *can't* safely construct any heap-based objects
   * by, say, passing them any of their data.  This is because the
   * act of allocating space for these objects may start a garbage
   * collection, which can move *other* objects.  So any references
   * passed into the constructor will be invalid; instead, the
   * process must save the data to be put in the new object in a
   * root location (such as the process stack) and after it is
   * given the newly-constructed object, it must store the data
   * straight from the root location.
   */

  Object::ref type(void) const {
    return Object::to_ref(symbol_cons);
  }

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(car_ref);
    gt->traverse(cdr_ref);
  }

  Object::ref len();

  void call(Process & proc, size_t & reductions);
};

/*
 * WARNING: It's possible to write something like:
 * car(foo) = 42;
 * THIS IS WRONG.  Avoid.
 * */
extern inline Object::ref const& car(Object::ref const& x) {
	if(!x) return x;
        return expect_type<Cons>(x,"'car expects a Cons cell")->car();
}
extern inline Object::ref const& cdr(Object::ref const& x) {
	if(!x) return x;
	return expect_type<Cons>(x,"'cdr expects a Cons cell")->cdr();
}
extern inline Object::ref scar(Object::ref c, Object::ref v) {
	return expect_type<Cons>(c,"'scar expects a true Cons cell")->scar(v);
}
extern inline Object::ref scdr(Object::ref c, Object::ref v) {
	return expect_type<Cons>(c,"'scdr expects a true Cons cell")->scdr(v);
}


/*-----------------------------------------------------------------------------
HlPid
-----------------------------------------------------------------------------*/
/*Represents a process that can
be sent messages to by this process
*/

class Process;

class HlPid : public GenericDerived<HlPid> {
public:
	Process* process;

	bool is(Object::ref o) const {
		HlPid* pp = maybe_type<HlPid>(o);
		if(pp) {
			return process == pp->process;
		} else return false;
	}
	void enhash(HashingClass* hc) const {
		hc->enhash((size_t) process);
	}
	Object::ref type(void) const {
		return Object::to_ref(symbol_pid);
	}
};

/*-----------------------------------------------------------------------------
Closures
-----------------------------------------------------------------------------*/

/*Closure
*/
class Closure : public GenericDerivedVariadic<Closure> {
private:
  Object::ref body;
  bool nonreusable;
  bool kontinuation;
public:
  Closure(size_t sz) : GenericDerivedVariadic<Closure>(sz), 
                       nonreusable(true) {}
  Object::ref& operator[](size_t i) { 
    if (i < size())
      return index(i);
    else
      throw_HlError("internal: trying to access closed vars with an index too large!");
  }
  Object::ref code() { return body; }
  void codereset(Object::ref b) { body = b; }
  void banreuse() {
    /*ban reuse for this and for every other continuation referred to*/
    Closure* kp = this;
  loop:
    kp->nonreusable = true;
    for(size_t i = 0; i < kp->sz; ++i) {
      Object::ref tmp = kp->index(i);
      if(is_a<Generic*>(tmp)) {
        Closure* cp = dynamic_cast<Closure*>(as_a<Generic*>(tmp));
        if(cp && cp->kontinuation) {
          /*assume only one continuation is actually referred to
          directly (this is what the compiler emits anyway)
          */
          kp = cp;
          goto loop;
        }
      }
    }
  }
  bool reusable() { return !nonreusable; }

  static Closure* NewKClosure(Heap & h, size_t n);
  static Closure* NewClosure(Heap & h, size_t n);

  Object::ref type(void) const {
    return Object::to_ref(symbol_fn);
  }

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(body);
    for(size_t i = 0; i < sz; ++i) {
      gt->traverse(index(i));
    }
  }
};

/*-----------------------------------------------------------------------------
Strings
-----------------------------------------------------------------------------*/

/*
hl strings are really PImpl, where the implementation is a
variadic array of UnicodeChar's.  Note however that the
implementation my be shared by several hl strings.

An implementation gets shared when a string is used as a key
to a table.  The table creates a new HlString with the same
implementation, then sets the "shared" flag of the
implementation.  When a string-modifying operation is
performed on the string, the string-modify detects the
"shared" flag and copies the implementation.
*/
class HlString : public GenericDerived<HlString> {
public:
	Object::ref impl;

	void traverse_references(GenericTraverser* gt) {
		gt->traverse(impl);
	}

	bool is(Object::ref) const;
	void enhash(HashingClass*) const;

	UnicodeChar ref(size_t i) const;
	size_t size(void) const;
	/*modifies a string:
		stack.top(3) = string
		stack.top(2) = UnicodeChar
		stack.top(1) = offset to modify
	*/
	static void sref(Heap& hp, ProcessStack& stack);

	/*creates a string from the characters on stack.top(N) to stack.top(1)*/
	static void stack_create(Heap& hp, ProcessStack& stack, size_t N);
	Object::ref type(void) const {
		return Object::to_ref(symbol_string);
	}

	static Object::ref length(Object::ref);
	static Object::ref string_ref(Object::ref, Object::ref);
};

class HlStringImpl : public GenericDerivedVariadic<HlStringImpl> {
public:
	bool shared;
	inline Object::ref& operator[](size_t i) {
		return index(i);
	}
	inline Object::ref const& operator[](size_t i) const {
		return index(i);
	}
	inline size_t size(void) const { return sz; };
	HlStringImpl(size_t sz)
		: GenericDerivedVariadic<HlStringImpl>(sz),
		shared(0) { }
	Object::ref type(void) const {
		return Object::to_ref(symbol_unspecified);
	}
};

inline UnicodeChar HlString::ref(size_t i) const {
	HlStringImpl& S = *known_type<HlStringImpl>(impl);
	if(i > S.size()) {
		throw_HlError("'string-ref index out of bounds");
	}
	return as_a<UnicodeChar>(S[i]);
}

inline size_t HlString::size(void) const {
	HlStringImpl& S = *known_type<HlStringImpl>(impl);
	return S.size();
}

inline Object::ref HlString::length(Object::ref o) {
	HlString& S = *expect_type<HlString>(o,
			"'string-length expects a string");
	return Object::to_ref((int) S.size());
}

inline Object::ref HlString::string_ref(Object::ref s, Object::ref i) {
	HlString& S = *expect_type<HlString>(s,
			"'string-ref expects a string for first argument");
	if(!is_a<int>(i)) {
		throw_HlError("'string-ref expects a number for second argument");
	}
	int I = as_a<int>(i);
	return Object::to_ref(S.ref(I));
}

/*-----------------------------------------------------------------------------
Floating point numbers
-----------------------------------------------------------------------------*/

class Float : public GenericDerived<Float> {
private:
  double val;
public:
  // construction
  static inline Float* mk(Heap & h, double val) {
    Float *f = h.create<Float>();
    f->val = val;
    return f;
  }
  // make a float that will live forever
  static inline Float* mkEternal(double val) {
    Float *f = new Float();
    f->val = val;
    return f;
  }
  inline double get() { return val; }
  Object::ref type(void) const {
    return Object::to_ref(symbol_float);
  }
  // Numbers are immutable
};

/*conversion*/
inline Object::ref i_to_f( Object::ref o, Process& proc ) {
	#ifdef DEBUG
		if(!is_a<int>(o)) {
			throw_HlError("'i-to-f expected int");
		}
	#endif
	return Object::to_ref<Generic*>(
		Float::mk(proc.heap(), (double) as_a<int>(o))
	);
}

inline Object::ref f_to_i( Object::ref o ) {
	#ifdef DEBUG
		Float* tmp = expect_type<Float>(o, "f-to-i expected float");
	#endif
	return Object::to_ref<int>(
		(int) known_type<Float>(o)->get()
	);
}

/*-----------------------------------------------------------------------------
Array
-----------------------------------------------------------------------------*/

class HlArray : public GenericDerivedVariadic<HlArray> {
public:
	void traverse_references(GenericTraverser* gt) {
		for(size_t i = 0; i < sz; ++i) {
			gt->traverse(index(i));
		}
	}
	size_t size(void) const {
		return sz;
	}
	Object::ref& operator[](size_t i) {
		return index(i);
	}
	Object::ref const& operator[](size_t i) const {
		return index(i);
	}
	Object::ref type(void) const {
		return Object::to_ref(symbol_array);
	}
	HlArray(size_t sz) : GenericDerivedVariadic<HlArray>(sz) { }
};

/*-----------------------------------------------------------------------------
Tables
-----------------------------------------------------------------------------*/

enum HlTableType {
	/*table is empty*/
	hl_table_empty,

	/*pairs are stored in a straight array:
		[0] = first key
		[1] = first value
		[2] = second key
		[3] = second value
		...
	*/
	hl_table_linear,

	/*values are stored in straight array
		[0] = value for key '0
		[1] = value for key '1
		[2] = value for key '2
		...
	*/
	hl_table_arrayed,

	/*pairs are stored in hash buckets.  When hashes collide,
	simply put in next hash bucket.
		[0] = (k . v) pair with (hash k) == 0
		[1] = (k . v) pair with (hash k) == 0 or (hash k) == 1
		...
		[N] = (k . v) pair with (hash k) == N
		[N+1] = (k . v) pair with (hash k) == N or (hash k) == N + 1
	*/
	hl_table_hashed
};

class HlTable : public GenericDerived<HlTable> {
private:
	/*number of entries in a linear table before we switch
	over to a hashed table
	*/
	static const size_t LINEAR_LEVEL = 8;

	/*Maximum numeric index when inserting a numeric
	key into an empty table, and we decide to use
	an arrayed table
	*/
	static const size_t ARRAYED_LEVEL = 4;

	/*used internally*/
	Object::ref* linear_lookup(Object::ref) const;
	Object::ref* arrayed_lookup(Object::ref) const;
	Object::ref* hashed_lookup(Object::ref) const;

	Object::ref* location_lookup(Object::ref) const;

public:
	Object::ref impl;
	HlTableType tbtype;
	size_t pairs;		/*number of key-value pairs*/

	void traverse_references(GenericTraverser* gt) {
		gt->traverse(impl);
	}

	inline Object::ref lookup(Object::ref k) const {
		Object::ref* op = location_lookup(k);
		if(op) return *op; else return Object::nil();
	}

	/*On entry:
		stack.top(1) = key
		stack.top(2) = value
		stack.top(3) = table (type not checked!)
	On return:
		stack.top(1) = value
	*/
	static void insert(Heap&, ProcessStack&);

	static void keys(Heap&, ProcessStack&);

	Object::ref type(void) const {
		return Object::to_ref(symbol_table);
	}

	HlTable(void) : impl(Object::nil()), tbtype(hl_table_empty) { }
};

/*-----------------------------------------------------------------------------
SharedVar's / Containers
-----------------------------------------------------------------------------*/

class SharedVar : public GenericDerived<SharedVar> {
public:
	Object::ref val;
	void traverse_references(GenericTraverser* gt) {
		gt->traverse(val);
	}

	Object::ref type(void) const {
		return Object::to_ref(symbol_container);
	}

	SharedVar(void) : val(Object::nil()) { }
};

inline Object::ref make_sv(Object::ref nval, Process& proc) {
	SharedVar* rv = proc.heap().create<SharedVar>();
	rv->val = nval;
	return Object::to_ref(rv);
}

inline Object::ref sv_ref(Object::ref sv) {
	return expect_type<SharedVar>(sv, "sv-ref expects a container")
		->val;
}

inline Object::ref sv_set(Object::ref sv, Object::ref nval) {
	expect_type<SharedVar>(sv, "sv-set expects a container")
		->val = nval;
	return nval;
}

/*-----------------------------------------------------------------------------
I/O Ports (async)
-----------------------------------------------------------------------------*/

class IPort : public GenericDerived<IPort> {
private:
  // function to be called when input is available
  Object::ref callback;
public: 
  IPort() {}
  
  void setCallback(Object::ref fn) { callback = fn; }
  
  void traverse_references(GenericTraverser *gt) {
    gt->traverse(callback);
  }
  
  Object::ref type(void) const {
    return Object::to_ref(symbol_iport);
  }
};

class OPort : public GenericDerived<IPort> {
private:
  // function to be called when port is ready to write
  Object::ref callback;
public: 
  OPort() {}
  
  void setCallback(Object::ref fn) { callback = fn; }
  
  void traverse_references(GenericTraverser *gt) {
    gt->traverse(callback);
  }
  
  Object::ref type(void) const {
    return Object::to_ref(symbol_oport);
  }
};

/*-----------------------------------------------------------------------------
Tagged types
-----------------------------------------------------------------------------*/

class HlTagged : public GenericDerived<HlTagged> {
public:
	/*NOTE! these two fields should be immutable after construction.
	This is because it would then allow is-comparison to be performed
	recursively wtihout worrying about cycles.
	*/
	Object::ref o_type;
	Object::ref o_rep;

	bool is(Object::ref o) const {
		HlTagged* htp = maybe_type<HlTagged>(o);
		if(htp) {
			/*!!WARNING: deep recursion might overflow C-stack!
			TODO: figure out a non-recursive algo
			*/
			return ::is(o_type, htp->o_type) &&
				::is(o_rep, htp->o_rep);
		} else return false;
	}
	void enhash(HashingClass* hc) const {
		hc->enhash(hash_is(o_type));
		hc->enhash(hash_is(o_rep));
	}

	void traverse_references(GenericTraverser* gt) {
		gt->traverse(o_type);
		gt->traverse(o_rep);
	}
	Object::ref type(void) const {
		return o_type;
	}
};

/*-----------------------------------------------------------------------------
Types
-----------------------------------------------------------------------------*/

extern inline Object::ref type(Object::ref ob) {
	if(is_a<Generic*>(ob)) {
		return as_a<Generic*>(ob)->type();
	}
	/*maybe some way of putting this in tag_traits?*/
	if(is_a<Symbol*>(ob)) {
		return Object::to_ref(symbol_sym);
	}
	if(is_a<int>(ob)) {
		return Object::to_ref(symbol_int);
	}
	if(is_a<UnicodeChar>(ob)) {
		return Object::to_ref(symbol_char);
	}
}

extern inline Object::ref rep(Object::ref ob) {
	HlTagged* htp = maybe_type<HlTagged>(ob);
	if(htp) {
		return htp->o_rep;
	} else return ob;
}

/*-----------------------------------------------------------------------------
Binary Objects
-----------------------------------------------------------------------------*/

/*Binary objects are just shared immutable arrays
of bytes.
*/
class BinObj : public GenericDerived<BinObj> {
private:
	boost::shared_ptr<std::vector<unsigned char> > pdat;
public:
	static inline BinObj* create(Heap& hp, boost::shared_ptr<std::vector<unsigned char> > const& np) {
		BinObj* rv = hp.create<BinObj>();
		rv->pdat = np;
		return rv;
	}
	unsigned char const& operator[](size_t i) const {
		return (*pdat)[i];
	}
	size_t size(void) const {
		return pdat->size();
	}

	Object::ref type(void) const {
		return Object::to_ref(symbol_binobj);
	}

};

#endif //TYPES_H

