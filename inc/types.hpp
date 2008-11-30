#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

#include<cstring>
#include"objects.hpp"
#include"generics.hpp"
#include"heaps.hpp"

class ProcessStack;

/*-----------------------------------------------------------------------------
Specialized broken heart tags
-----------------------------------------------------------------------------*/

/*The BrokenHeartFor classes are specialized for each
generic-derived class.  They should *not* add any
storage.
*/
template<class T>
class BrokenHeartFor : public BrokenHeart {
private:
	// disallowed
	BrokenHeartFor<T>();
	BrokenHeartFor<T>(BrokenHeartFor<T> const&);
public:
	virtual size_t real_size(void) const {
		return compute_size<T>();
	}
	explicit BrokenHeartFor<T>(Generic* x) : BrokenHeart(x) { }
};

template<class T>
class BrokenHeartForVariadic : public BrokenHeartVariadic {
private:
	// disallowed
	BrokenHeartForVariadic<T>();
	BrokenHeartForVariadic<T>(BrokenHeartForVariadic<T> const&);
public:
	virtual size_t real_size(void) const {
		return compute_size_variadic<T>(sz);
	}
	BrokenHeartForVariadic<T>(Generic* x, size_t nsz)
		: BrokenHeartVariadic(x, nsz) { }
};

/*-----------------------------------------------------------------------------
Base classes for Generic-derived objects
-----------------------------------------------------------------------------*/

template<class T>
class GenericDerived : public Generic {
protected:
	GenericDerived<T>(void) { }
public:
	virtual size_t real_size(void) const {
		return compute_size<T>();
	}
	virtual Generic* clone(Semispace* nsp) const {
		void* pt = nsp->alloc(real_size());
		try {
			new(pt) T(*static_cast<T const*>(this));
			return (Generic*) pt;
		} catch(...) {
			nsp->dealloc(pt);
			throw;
		}
	}
	virtual void break_heart(Generic* to) {
		Generic* gp = this;
		gp->~Generic();
		new((void*) gp) BrokenHeartFor<T>(to);
	};
};

/*This class implements a variable-size object by informing the
memory system to reserve extra space.
Note that classes which derive from this should provide
a factory function!
*/
template<class T>
class GenericDerivedVariadic : public Generic {
	GenericDerivedVariadic<T>(void); // disallowed!
protected:
	/*number of extra Object::ref's*/
	size_t sz;
	/*used by the derived classes to get access to
	the variadic data at the end of the object.
	*/
	inline Object::ref& index(size_t i) const {
		void* vp = const_cast<GenericDerivedVariadic<T>*>(this);
		char* cp = (char*) vp;
		cp = cp + sizeof(T);
		Object::ref* op = (Object::ref*) cp;
		return op[i];
	}
	explicit GenericDerivedVariadic<T>(size_t nsz) : sz(nsz) {
		/*clear the extra references*/
		for(size_t i = 0; i < nsz; ++i) {
			index(i) = Object::nil();
		}
	}
	GenericDerivedVariadic<T>(GenericDerivedVariadic<T> const& o)
		: sz(o.sz) {
		for(size_t i = 0; i < sz; ++i) {
			index(i) = o.index(i);
		}
	}
public:
	virtual size_t real_size(void) const {
		return compute_size_variadic<T>(sz);
	}
	virtual Generic* clone(Semispace* nsp) const {
		void* pt = nsp->alloc(real_size());
		try {
			new(pt) T(*static_cast<T const*>(this));
			return (Generic*) pt;
		} catch(...) {
			nsp->dealloc(pt);
			throw;
		}
	}
	virtual void break_heart(Generic *to) {
		Generic* gp = this;
		size_t nsz = sz; //save this before dtoring!
		gp->~Generic();
		new((void*) gp) BrokenHeartForVariadic<T>(to, nsz);
	}
        virtual size_t size() {
                return sz;
        }
};

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

void throw_HlError(char const*);

/*Usage:
Cons* cp = expect_type<Cons>(proc.stack().top(),
		"Your mom expects a Cons cell on top"
);
*/
template<class T>
static inline T* expect_type(Object::ref x, char const* error) {
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

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(car_ref);
    gt->traverse(cdr_ref);
  }
};

extern inline Object::ref car(Object::ref x) {
	if(!x) return x;
        return expect_type<Cons>(x,"'car expects a Cons cell")->car();
}
extern inline Object::ref cdr(Object::ref x) {
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
};

/*-----------------------------------------------------------------------------
Closures
-----------------------------------------------------------------------------*/

struct bytecode_t;

/*Closure
*/
class Closure : public GenericDerivedVariadic<Closure> {
private:
  bytecode_t* body;
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
  bytecode_t* code() { return body; }
  void codereset(bytecode_t *b) { body = b; }
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
  static Closure* NewKClosure(Heap & h, bytecode_t *body, size_t n);
  static Closure* NewClosure(Heap & h, bytecode_t *body, size_t n);

  void traverse_references(GenericTraverser *gt) {
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

bool HlString::is(Object::ref o) const {
	HlString* hs = maybe_type<HlString>(o);
	if(hs) {
		if(hs->impl == impl) return true;
		if(hs->size() != size()) return false;
		HlStringImpl& S1 = *known_type<HlStringImpl>(impl);
		HlStringImpl& S2 = *known_type<HlStringImpl>(hs->impl);
		for(size_t i = 0; i < size(); ++i) {
			if(S1[i] != S2[i]) {
				return false;
			}
		}
		return true;
	} else return false;
}
void HlString::enhash(HashingClass* hc) const {
	if(impl) {
		HlStringImpl& S = *known_type<HlStringImpl>(impl);
		for(size_t i = 0; i < size(); ++i) {
			hc->enhash(as_a<UnicodeChar>(S[i]).dat);
		}
	}
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
  // Numbers are immutable
};

/*-----------------------------------------------------------------------------
Array
-----------------------------------------------------------------------------*/

class HlArray : public GenericDerivedVariadic<HlArray> {
public:
	void traverse(GenericTraverser* gt) {
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
	HlTableType type;
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

	HlTable(void) : impl(Object::nil()), type(hl_table_empty) { }
};

#endif //TYPES_H

