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
public:
	Generic* to;
	virtual bool break_heart(Generic* to) {
		/*already broken - don't break too much!*/
		throw_OverBrokenHeart(to);
	}
	BrokenHeart(Generic* nto) : to(nto) { }
};

template<class T>
class BrokenHeartFor : public BrokenHeart {
public:
	virtual bool real_size(void) const {
		return sizeof(T);
	}
	explicit BrokenHeartFor<T>(Generic* x) : BrokenHeart(x) { }
};

template<class T>
class BrokenHeartForVariadic : public BrokenHeart {
private:
	size_t sz;
public:
	virtual bool real_size(void) const {
		return sizeof(T) + sz * sizeof(Object::ref);
	}
	BrokenHeartForVariadic<T>(Generic* x, size_t nsz)
		: BrokenHeart(x), sz(nsz) { }
};

/*-----------------------------------------------------------------------------
Base classes for Generic-derived objects
-----------------------------------------------------------------------------*/

template<class T>
class GenericDerived : public Generic {
public:
	virtual size_t real_size(void) const {
		return sizeof(T);
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
	GenericDerivedVariadic<T>(); // disallowed!
protected:
	/*number of extra Object::ref's*/
	size_t sz;
	/*used by the derived classes to get access to
	the variadic data at the end of the object.
	*/
	Object::ref& index(size_t i) {
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
		return sizeof(T) + sz * sizeof(Object::ref);
	}
	virtual void break_heart(Object::ref to) {
		Generic* gp = this;
		size_t nsz = sz; //save this before dtoring!
		gp->~Generic();
		new((void*) gp) BrokenHeartForVariadic<T>(to, nsz);
	}
};


#endif //TYPES_H

