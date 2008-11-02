#ifndef HEAPS_H
#define HEAPS_H

#include"objects.hpp"

class Generic;
class SharedVar;

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

class Semispace {
private:
	void* mem;
	void* allocpt;
	void* svallocpt; // sharedvar allocation point
	size_t prev_alloc;
	size_t max;
public:
	Semispace(size_t);
	~Semispace();
	void* alloc(size_t);
	void dealloc(void*);
	SharedVar* sv_alloc(void);
	void sv_dealloc(SharedVar*);
	void resize(size_t);
	bool can_fit(size_t) const;

	size_t size(void) const { return max; };
	size_t used(void) const {
		return (size_t)(((char*) allocpt) - ((char*) mem))
			+ (size_t)((((char*) mem) + max) - ((char*) svallocpt));
	};

	std::pair<boost::shared_ptr<Semispace>, Generic* >
		clone(Generic*) const;

	friend class Heap;
};

/*last-in-first-out allocation semispace*/
class LifoSemispace {
private:
	void* mem;
	void* allocpt;
	void* end;
	size_t prevalloc;
public:
	LifoSemispace(size_t sz);
	~LifoSemispace();
	void* alloc(size_t sz);
	void dealloc(void*); // used only in a constructor-fail delete
	void normal_dealloc(Generic*);
	bool can_fit(size_t) const;
	size_t size(void) const {
		return (size_t) (((char*) end) - ((char*) mem));
	}
	size_t used(void) const {
		return (size_t) (((char*) end) - ((char*) allocpt));
	}
	/*no need to clone LIFO semispaces: they never get passed around*/

	friend class Heap;
};

#endif //HEAPS_H

