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

	/*Copies an object, used for message passing and
	copying garbage collection
	*/
	virtual Generic* clone(Semispace*) const =0;

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
	virtual Generic* clone(Semispace*) const {
		/*broken - why are we cloning this?*/
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

#endif // GENERICS_H
