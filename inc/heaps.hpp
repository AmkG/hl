#ifndef HEAPS_H
#define HEAPS_H

#include"objects.hpp"
#include"generics.hpp"

#include<cstring>
#include<utility>

#include<boost/scoped_ptr.hpp>
#include<boost/noncopyable.hpp>

class Generic;
class ValueHolder;

void throw_DeallocError(void*);

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

class Semispace : boost::noncopyable {
private:
	void* mem;
	void* allocstart;
	void* allocpt;
	void* lifoallocstart;
	void* lifoallocpt;
	size_t prev_alloc;
	size_t max;
public:
	explicit Semispace(size_t);
	~Semispace();

	void* alloc(size_t);
	void dealloc(void*);

	void* lifo_alloc(size_t);
	void lifo_dealloc(void*);
	void lifo_dealloc_abort(void*);

	inline size_t size(void) const { return max; };
	inline size_t used(void) const {
		return (size_t)(((char*) allocpt) - ((char*) mem)) +
			(size_t)
			((((char*) mem) + max) - ((char*) lifoallocpt));
	};
	inline size_t free(void) const {
		return (size_t)(((char*) lifoallocpt) - ((char*) allocpt));
	}
	inline bool can_fit(size_t sz) const {
		return sz <= free();
	}

	void clone(boost::scoped_ptr<Semispace>&, Generic*&) const;

	friend class Heap;
};

/*-----------------------------------------------------------------------------
Heaps
-----------------------------------------------------------------------------*/

class Heap : boost::noncopyable {
private:
	boost::scoped_ptr<Semispace> main;
	bool tight;

protected:
	boost::scoped_ptr<ValueHolder> other_spaces;
	virtual void scan_root_object(GenericTraverser*) =0;
	void GC(size_t);

public:
	template<class T>
	inline T* create(void) {
		/*compile-time checking that T inherits from Generic*/
		Generic* _create_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		size_t sz = compute_size<T>();
		if(!main->can_fit(sz)) GC(sz);
		void* pt = main->alloc(sz);
		try {
			new(pt) T();
			return pt;
		} catch(...) {
			main->dealloc(pt);
			throw;
		}
	}
	template<class T>
	inline T* create_variadic(size_t extra) {
		/*TODO: consider splitting Generic hierarchy into two
		hierarchies inheriting from Generic, one hierarchy for
		non-variadic types, the other hierarchy for variadic
		types.
		*/
		Generic* _create_variadic_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		size_t sz = compute_size_variadic<T>(extra);
		if(!main->can_fit(sz)) GC(sz);
		void* pt = main->alloc(sz);
		try {
			new(pt) T(extra);
			return pt;
		} catch(...) {
			main->dealloc(pt);
			throw;
		}
	}
};

#endif //HEAPS_H

