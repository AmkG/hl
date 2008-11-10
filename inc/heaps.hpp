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
ValueHolders
-----------------------------------------------------------------------------*/

class ValueHolder;

void throw_ValueHolderLinkingError(ValueHolder*);

class ValueHolder {
private:
	boost::scoped_ptr<Semispace> sp;
	Object::ref val;

	/*
	Allow chaining, for example in
	the mailbox of a process.
	*/
	boost::scoped_ptr<ValueHolder> next;

	ValueHolder() { }
public:
	typedef boost::scoped_ptr<ValueHolder> ptr;

	inline size_t used_total(void) const {
		size_t total = 0;
		for(ValueHolder const* pt = this; pt; pt = &*pt->next) {
			total += pt->sp->used();
		}
		return total;
	}

	/* insert(what, to)
	On entry:
		what is a pointer to a ValueHolder
		to is a potentially empty list of
		  ValueHolder's
	On exit:
		what is an empty pointer
		to is the new list
	*/
	static inline void insert(ptr& what, ptr& to) {
		#ifdef DEBUG
			/*make sure what to insert exists and doesn't
			have a next
			*/
			if(!what || what->next) {
				throw_ValueHolderLinkingError(&*what);
			}
		#endif
		what->next.swap(to);
		what.swap(to);
	}
	/* remove(what, from)
	On entry:
		what is an empty pointer
		from is a non-empty list of ValueHolders
	On exit:
		what is the first element in the list
		from is the rest of the list, or empty if the list
		  only had one element
	*/
	static inline void remove(ptr& what, ptr& from) {
		#ifdef DEBUG
			/*make sure there's something to remove and that
			what is empty
			*/
			if(!from || what) throw_ValueHolderLinkingError(&*from);
		#endif
		what.swap(from);
		from.swap(what->next);
	}
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
	void cheney_collection(Semispace*);
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
			return (T*) pt;
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

