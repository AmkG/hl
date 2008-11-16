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
HeapTraverser
-----------------------------------------------------------------------------*/

class HeapTraverser {
public:
	virtual void traverse(Generic*) =0;
	virtual ~HeapTraverser(void) { }
};

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

	void traverse_objects(HeapTraverser*) const;

	void clone(boost::scoped_ptr<Semispace>&, Generic*&) const;

	friend class Heap;
};

/*-----------------------------------------------------------------------------
ValueHolders
-----------------------------------------------------------------------------*/

class ValueHolder;

void throw_ValueHolderLinkingError(ValueHolder*);

class ValueHolder;
class LockedValueHolderRef;

/*smart pointer specifically for ValueHolder*/
class ValueHolderRef : boost::noncopyable {
private:
	ValueHolder* p;
public:
	ValueHolderRef(ValueHolder* np = 0) : p(np) { }
	/*don't define this here: just declare.
	The definition should occur after ValueHolder
	is completely declared.
	*/
	~ValueHolderRef();
	void reset(ValueHolder* np = 0);
	void swap(ValueHolderRef& n) {
		ValueHolder* tmp = p;
		p = n.p;
		n.p = tmp;
	}
	ValueHolder* operator->(void) { return p; }
	ValueHolder const* operator->(void) const { return p; }
	ValueHolder& operator*(void) { return *p; }
	ValueHolder const& operator*(void) const { return *p; }
	bool empty(void) const { return p == 0; }

	void insert(ValueHolderRef&);
	void remove(ValueHolderRef&);

	Object::ref value(void);

	friend class LockedValueHolderRef;
};

class ValueHolder {
private:
	boost::scoped_ptr<Semispace> sp;
	Object::ref val;

	/*
	Allow chaining, for example in
	the mailbox of a process.
	*/
	ValueHolderRef next;

	ValueHolder() { }
public:

	inline size_t used_total(void) const {
		size_t total = 0;
		for(ValueHolder const* pt = this; pt; pt = &*pt->next) {
			total += pt->sp->used();
		}
		return total;
	}

	void traverse_objects(HeapTraverser*) const;
	friend class ValueHolderRef;
	friend class LockedValueHolderRef;
};

inline ValueHolderRef::~ValueHolderRef() {
	delete p;
}
inline void ValueHolderRef::reset(ValueHolder* np) {
	delete p;
	p = np;
}

inline void ValueHolderRef::insert(ValueHolderRef& o) {
	#ifdef DEBUG
		/*make sure there's exactly one element to insert*/
		if(!o.p) {
			throw_ValueHolderLinkingError(o.p);
		}
		if(o->next.p) {
			throw_ValueHolderLinkingError(o.p);
		}
	#endif
	o->next.p = p;
	p = o.p;
	o.p = 0; /*release*/
}

inline void ValueHolderRef::remove(ValueHolderRef& o) {
	#ifdef DEBUG
		if(o.p) {
			throw_ValueHolderLinkingError(o.p);
		}
	#endif
	if(!p) return;
	o.p = p;
	p = p->next.p;
	o->next.p = 0;
}

inline Object::ref ValueHolderRef::value(void) {
	if(p)	return p->val;
	else	return Object::nil();
}

/*-----------------------------------------------------------------------------
Heaps
-----------------------------------------------------------------------------*/

class Heap : boost::noncopyable {
private:
	boost::scoped_ptr<Semispace> main;
	bool tight;

protected:
	ValueHolderRef other_spaces;
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
			return (T*)pt;
		} catch(...) {
			main->dealloc(pt);
			throw;
		}
	}

	void traverse_objects(HeapTraverser*) const;

	explicit Heap(size_t initsize = 8 * sizeof(Object::ref))
		: main(new Semispace(initsize)), tight(1) { }
};

#endif //HEAPS_H

