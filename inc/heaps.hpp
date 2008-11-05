#ifndef HEAPS_H
#define HEAPS_H

#include"objects.hpp"

#include<cstring>
#include<utility>

#include<boost/shared_ptr.hpp>

class Generic;
class SharedVar;

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

class Semispace {
private:
	void* mem;
	void* allocpt;
	void* lifoallocpt;
	size_t prev_alloc;
	size_t max;
public:
	Semispace(size_t);
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

#endif //HEAPS_H

