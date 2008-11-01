#ifndef TYPES_H
#define TYPES_H
/*
Defines a set of types for use on the hl-side.
*/

#include"objects.hpp"

class GenericTraverser {
public:
	virtual void traverse(Object::ref) =0;
	virtual ~GenericTraverser();
};

/*-----------------------------------------------------------------------------
SharedVar
-----------------------------------------------------------------------------*/

/*
Handled specially: Semispace is allocated on two ends,
the lower end for Generic objects, the upper end for
SharedVar's.
*/
class SharedVar {
public:
	/*ends up being just one pointer*/
	Object::ref value;
	SharedVar(Object::ref x) : value(x) { }
	static inline void traverse_references(GenericTraverser* gt) {
		gt->traverse(value);
	}
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
	virtual size_t real_size(void) =0;

	/*hash functions for table-ident and table-is*/
	virtual size_t hash_ident(void) {
		return reinterpret_cast<size_t>(this);
	}
	virtual size_t hash_is(void) {
		return reinterpret_cast<size_t>(this);
	}

	/*dtor*/
	virtual ~Generic() { }
};


#endif //TYPES_H

