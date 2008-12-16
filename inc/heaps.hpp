#ifndef HEAPS_H
#define HEAPS_H

#include"objects.hpp"
#include"generics.hpp"

#include<cstring>
#include<utility>
#include<set>

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

/*
Utility class to wrap a GenericTraverser in a
HeapTraverser
*/
template<class T>
class ObjectsTraverser : public HeapTraverser {
private:
	T t;
public:
	template<class U>
	ObjectsTraverser(U u) : t(u) {
		GenericTraverser* ObjectsTraverser_template_should_derive_GenericTraverser
			= static_cast<GenericTraverser*>(&t);
	}
	template<class U, class V>
	ObjectsTraverser(U u, V v) : t(u, v) { }

	void traverse(Generic* gp) {
		gp->traverse_references(&t);
	}
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
	void lifo_dealloc(Generic*);
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
	friend class ValueHolder;
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
			if(pt->sp) total += pt->sp->used();
		}
		return total;
	}

	void clone(ValueHolderRef&) const;

	void traverse_objects(HeapTraverser*) const;

	static void copy_object(ValueHolderRef&, Object::ref);

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

	#ifndef ONLY_COPYING_GC
		enum GCType { gc_type_copying, gc_type_generational };
		GCType gc_type;

		/*Deferred GC type.*/
		GCType recommended_gc_type;
		/*
		We first decide to change GC types, but only
		assign to recommended_gc_type.  When the ssb
		is acquired, only then do we finalize the
		recommended type to the actual type.
		This is done in order to facilitate JIT.
		We can then JIT two versions of the code:
		one with write barriers, another without
		write barriers.  When JITted code ends up
		allocating a lot of memory while the GC is
		a copying GC, we would rather switch over to
		the generational mark and compact one.
		However the JITted code at that time may not
		have the required write barrier.
		We can only safely switch collectors when
		JITted code isn't running.
		*/

		static const size_t GENERATIONAL_TRIGGER_LEVEL =
			16384 * sizeof(Object::ref);
		static const size_t SSB_SIZE = 512;

		void* ssb_start;
		void* ssb_point;

		void* old_end;

		void generational_GC(size_t);

		std::set<Object::ref*> intergen;

	#endif

	void cheney_collection(Semispace*);
	void default_GC(size_t);

protected:
	ValueHolderRef other_spaces;
	void GC(size_t);

	void free_heap(void) {
		main.reset();
		other_spaces.reset(0);
	}

	/*required overload*/
	/*This virtual function *must* be defined by any derived
	class.  It should scan the root set of the heap.
	*/
	virtual void scan_root_object(GenericTraverser*) =0;

public:

	#ifndef ONLY_COPYING_GC
		Object::ref** acquire_ssb(void) const;
		void release_ssb(Object::ref**);

		/*determine if the specified address is in old space*/
		bool in_old(void*);

		friend Object::ref** __ssb_clean(Object::ref**);
	#endif

	template<class T>
	inline T* create(void) {
		/*compile-time checking that T inherits from Generic*/
		Generic* _create_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		if(!main) throw std::bad_alloc();
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
		if(!main) throw std::bad_alloc();
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
	template<class T>
	inline T* lifo_create(void) {
		Generic* _lifo_create_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		size_t sz = compute_size<T>();
		if(!main->can_fit(sz)) GC(sz);
		void* pt = main->lifo_alloc(sz);
		try {
			new(pt) T();
			return (T*) pt;
		} catch(...) {
			main->lifo_dealloc_abort(pt);
			throw;
		}
	}
	template<class T>
	inline T* lifo_create_variadic(size_t extra) {
		Generic* _lifo_create_variadic_template_must_be_Generic_ =
			static_cast<Generic*>((T*) 0);
		size_t sz = compute_size_variadic<T>(extra);
		if(!main->can_fit(sz)) GC(sz);
		void* pt = main->lifo_alloc(sz);
		try {
			new(pt) T(extra);
			return (T*)pt;
		} catch(...) {
			main->lifo_dealloc_abort(pt);
			throw;
		}
	}

	inline void lifo_dealloc(Generic* gp) {
		main->lifo_dealloc(gp);
	}

	void traverse_objects(HeapTraverser*) const;

	explicit Heap(size_t initsize = 8 * sizeof(Object::ref))
		: main(new Semispace(initsize)),
		tight(1)
		#ifndef ONLY_COPYING_GC
			, gc_type(gc_type_copying),
			recommended_gc_type(gc_type_copying),
			ssb_start(0),
			ssb_point(0),
			ssb_end(0),
			old_end(0),
			intergen()
		#endif
	{ }
        virtual ~Heap() {}
};

#endif //HEAPS_H

