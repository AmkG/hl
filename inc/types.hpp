#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

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
	virtual void break_heart(Object::ref) =0;

	/*dtor*/
	virtual ~Generic() { }
};

template<class T>
class GenericDerived : class Generic {
public:
	virtual bool real_size(void) {
		return sizeof(T);
	}
	virtual void break_heart(Generic* to) {
		Generic* gp = this;
		gp->~Generic();
		new((void*) gp) BrokenHeartFor<T>(to);
	};
};

void throw_OverBrokenHeart(Generic*);

class BrokenHeart : class Generic {
public:
	Generic* to;
	virtual bool break_heart(Generic* to) {
		/*already broken - don't break too much!*/
		throw_OverBrokenHeart(to);
	}
};

template<class T>
class BrokenHeartFor : class BrokenHeart {
public:
	virtual bool real_size(void) {
		return sizeof(T);
	}
};

#endif //TYPES_H

