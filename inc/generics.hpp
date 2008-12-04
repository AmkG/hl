#ifndef GENERICS_H
#define GENERICS_H

#include"objects.hpp"

class Semispace;

/*-----------------------------------------------------------------------------
GenericTraverser
-----------------------------------------------------------------------------*/
/*An abstract base class which provides a
"traverse" method.  This method is invoked
on each reference of an object.
*/
class GenericTraverser {
public:
	virtual void traverse(Object::ref&) =0;
	virtual ~GenericTraverser() {}
};

/*-----------------------------------------------------------------------------
HashingClass
-----------------------------------------------------------------------------*/
/*A class which provides a "enhash" method, which
perturbs its internal state.
*/
class HashingClass {
private:
	uint32_t a, b;
public:
	uint32_t c;
	void enhash(size_t);
	HashingClass(void);
};

/*-----------------------------------------------------------------------------
Generic
-----------------------------------------------------------------------------*/

class Generic {
public:

	virtual void traverse_references(GenericTraverser* gt) {
		/*default to having no references to traverse*/
		/*example for Cons:
		gt->traverse(a);
		gt->traverse(d);
		*/
	}

	/*some objects have extra allocated space at their ends
	(e.g. Closure).
 	This virtual function returns the total size of the class
	plus extra allocated space.
	*/
	virtual size_t real_size(void) const =0;

	/*Copies an object, used for message passing and
	copying garbage collection
	*/
	virtual Generic* clone(Semispace*) const =0;

	/*broken hearts for GC*/
	virtual void break_heart(Generic*) =0;

	/*------These two functions must be redefined together------*/
	virtual bool is(Object::ref) const {
		/*return true if the given Object::ref is-equal to
		this object, even if they have different addresses
		*/
		return false;
	}
	virtual void enhash(HashingClass* hc) const {
		/*Should provide hc with all the values
		it uses to determine equality, as in is()
		above.
		Should *not* provide Generic* pointers,
		because those *will* change in a GC, and
		we want hash tables to work even across
		a GC.
		*/
		return;
	}
	/*------These two functions must be redefined together------*/

	/*dtor*/
	virtual ~Generic() { }
};

/*-----------------------------------------------------------------------------
Broken Heart tags
-----------------------------------------------------------------------------*/

void throw_OverBrokenHeart(Generic*);

class BrokenHeart : public Generic {
private:
	// disallowed
	BrokenHeart();
	BrokenHeart(BrokenHeart const&);
public:
	Generic* to;
	virtual void break_heart(Generic* to) {
		/*already broken - don't break too much!*/
		throw_OverBrokenHeart(to);
	}
	virtual Generic* clone(Semispace*) const {
		/*broken - why are we cloning this?*/
		throw_OverBrokenHeart(to);
                return NULL;
	}
	BrokenHeart(Generic* nto) : to(nto) { }
};

class BrokenHeartVariadic : public BrokenHeart {
private:
	// disallowed
	BrokenHeartVariadic();
	BrokenHeartVariadic(BrokenHeart const&);
protected:
	size_t sz;
public:
	BrokenHeartVariadic(Generic* x, size_t nsz)
		: BrokenHeart(x), sz(nsz) { }
};

/*-----------------------------------------------------------------------------
Size computations
-----------------------------------------------------------------------------*/

template<class T>
static inline size_t compute_size(void) {
	size_t sizeofBrokenHeart = Object::round_up_to_alignment(
			sizeof(BrokenHeart)
	);
	size_t sizeofT = Object::round_up_to_alignment(sizeof(T));
	return
	(sizeofT < sizeofBrokenHeart) ?		sizeofBrokenHeart :
	/*otherwise*/				sizeofT ;
}

template<class T>
static inline size_t compute_size_variadic(size_t sz) {
	/*remember that this is variadic*/
	size_t sizeofBrokenHeart = Object::round_up_to_alignment(
			sizeof(BrokenHeartVariadic)
	);
	size_t sizeofT = Object::round_up_to_alignment(sizeof(T))
			 + Object::round_up_to_alignment(
				sz * sizeof(Object::ref)
		);
	return
	(sizeofT < sizeofBrokenHeart) ?		sizeofBrokenHeart :
	/*otherwise*/				sizeofT ;
}

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

static inline bool is(Object::ref a, Object::ref b) {
	if(a == b) return true;
	if(!is_a<Generic*>(a)) return false;
	return as_a<Generic*>(a)->is(b);
}

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
                if (i >= sz)
                  throw_HlError
                    ("internal: overflow in GenericDerivedVariadic index");
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

#endif // GENERICS_H

