#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

#include<cstring>
#include"objects.hpp"

class GenericTraverser {
public:
	virtual void traverse(Object::ref&) =0;
	virtual ~GenericTraverser();
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

	/*hash functions for table-ident and table-is*/
	virtual size_t hash_ident(void) const {
		return reinterpret_cast<size_t>(this);
	}
	virtual size_t hash_is(void) const {
		return reinterpret_cast<size_t>(this);
	}

	/*broken hearts for GC*/
	virtual void break_heart(Generic*) =0;

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
	/*exists only for RTTI*/
	virtual void break_heart(Generic* to) {
		throw_OverBrokenHeart(to);
	}
	BrokenHeartVariadic(Generic* x, size_t nsz)
		: BrokenHeart(x), sz(nsz) { }
};

template<class T>
class BrokenHeartFor : public BrokenHeart {
private:
	// disallowed
	BrokenHeartFor<T>();
	BrokenHeartFor<T>(BrokenHeartFor<T> const&);
public:
	virtual size_t real_size(void) const {
		return Object::round_up_to_alignment(sizeof(T));
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
		return Object::round_up_to_alignment(sizeof(T))
			 + Object::round_up_to_alignment(
				sz * sizeof(Object::ref)
			)
		;
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
		return Object::round_up_to_alignment(sizeof(T));
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
	inline Object::ref& index(size_t i) {
		void* vp = this;
		char* cp = (char*) vp;
		cp = cp + sizeof(T);
		Object::ref* op = (void*) cp;
		return op[i];
	}
	GenericDerivedVariadic<T>(size_t nsz) : sz(nsz) {
		/*clear the extra references*/
		for(size_t i; i < nsz; ++i) {
			index(i) = Object::nil();
		}
	}
public:
	virtual size_t real_size(void) const {
		return Object::round_up_to_alignment(sizeof(T))
			 + Object::round_up_to_alignment(
				sz * sizeof(Object::ref)
			)
		;
	}
	virtual void break_heart(Object::ref to) {
		Generic* gp = this;
		size_t nsz = sz; //save this before dtoring!
		gp->~Generic();
		new((void*) gp) BrokenHeartForVariadic<T>(to, nsz);
	}
};

/*-----------------------------------------------------------------------------
Utility
-----------------------------------------------------------------------------*/

void throw_HlError(char const*);

/*Usage:
Cons* cp = expect_type<Cons*>(proc.stack().top(),
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


/*
 * Cons cell
 */

class Cons : public GenericDerived<Cons> {
private:
  
  Object::ref car_ref;
  Object::ref cdr_ref;

public:

  inline Object::ref car() { return car_ref; }
  inline Object::ref cdr() { return cdr_ref; }

  Cons() : car_ref(Object::nil()), cdr_ref(Object::nil()) {}

  void traverse_references(GenericTraverser *gt) {
    gt->traverse(car_ref);
    gt->traverse(cdr_ref);
  }
};

static inline Object::ref car(Object::ref x) {
	if(!x) return x;
	return expect_type<Cons*>(x,"'car expects a Cons cell")->car();
}
static inline Object::ref cdr(Object::ref x) {
	if(!x) return x;
	return expect_type<Cons*>(x,"'cdr expects a Cons cell")->cdr();
}

#endif //TYPES_H
