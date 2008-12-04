#ifndef SPECIALIZEDS_H
#define SPECIALIZEDS_H

#include"generics.hpp"
#include"heaps.hpp"

void throw_HlError(char const*);

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



#endif // SPECIALIZEDS_H

