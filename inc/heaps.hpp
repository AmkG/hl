#ifndef HEAPS_H
#define HEAPS_H

#include"objects.hpp"

#include<cstring>
#include<utility>

#include<boost/scoped_ptr.hpp>
#include<boost/noncopyable.hpp>

class Generic;
class ValueHolder;

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

class Semispace : boost::noncopyable {
private:
	void* mem;
	void* allocpt;
	void* lifoallocpt;
	size_t prev_alloc;
	size_t max;
public:
	explicit Semispace(size_t);
	~Semispace();

	void* alloc(size_t);
	void dealloc(void*);

	void* lifo_alloc(void);
	void lifo_dealloc(void*);
	void lifo_dealloc_abort(void*);

	void resize(size_t);
	bool can_fit(size_t) const;

	size_t size(void) const { return max; };
	size_t used(void) const {
		return (size_t)(((char*) allocpt) - ((char*) mem)) +
			(size_t)
			((((char*) mem) + max) - ((char*) lifoallocpt));
	};

	void clone(boost::scoped_ptr<Semispace>&, Generic*&) const;

	friend class Heap;
};

/*-----------------------------------------------------------------------------
Heaps
-----------------------------------------------------------------------------*/

class Heap : boost::noncopyable {
private:
	boost::scoped_ptr<Semispace> main;
	boost::scoped_ptr<ValueHolder> other_spaces;

	void GC(void);

protected:
	virtual Object::ref root_object(void) const =0;

public:
	template<class T>
	inline T* create(void) {
		/*compile-time checking that T inherits from Generic*/
		Generic* _create_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		size_t sz = Object::round_up_to_alignment(sizeof(T));
		if(!main->can_fit(sz)) GC();
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
		size_t sz = Object::round_up_to_alignment(sizeof(T))
			+ Object::round_up_to_alignment(
				extra * sizeof(Object::ref)
			)
		;
		if(!main->can_fit(sz)) GC();
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

