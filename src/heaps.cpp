#include"all_defines.hpp"
#include"objects.hpp"
#include"heaps.hpp"
#include"types.hpp"

#include<cstdlib>
#include<stdint.h>

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

Semispace::Semispace(size_t nsz)
	: max(nsz) {
	mem = std::malloc(nsz);
	if(!mem) throw std::bad_alloc();

	allocpt = mem;
	// check alignment
	intptr_t tmp = reinterpret_cast<intptr_t>(allocpt);
	if(tmp & Object::tag_mask) {
		char* callocpt = allocpt;
		callocpt += Object::alignment - (tmp & Object::tag_mask);
		allocpt = callocpt;
	}

	char* cmem = mem;
	char* clifoallocpt = cmem + nsz;
	// adjust for alignment
	tmp = reinterpret_cast<intptr_t>(lifoallocpt);
	clifoallocpt -= (tmp & Object::tag_mask);

	lifoallocpt = clifoallocpt;
}

Semispace::~Semispace() {
	Generic* gp;
	size_t step;
	char* mvpt;
	char* endpt;

	/*TODO: consider refactoring*/
	/*----Delete normally-allocated memory----*/
	mvpt = mem;
	intptr_t tmp = reinterpret_cast<intptr_t>(mem);
	if(tmp & Object::tag_mask) {
		char* cmem = mem;
		cmem += Object::alignment - (tmp & Object::tag_mask);
		mvpt = cmem;
	}
	endpt = allocpt;

	while(mvpt < allocpt) {
		gp = (Generic*)(void*) mvpt;
		step = gp->real_size();
		gp->~Generic();
		mvpt += step;
	}

	/*----Delete lifo-allocated memory----*/
	mvpt = lifoallocpt;
	endpt = mem;
	endpt += sz;
	intptr_t tmp = reinterpret_cast<intptr_t>(endpt);
	endpt -= (tmp & Object::tag_mask);

	while(mvpt < allocpt) {
		gp = (Generic*)(void*) mvpt;
		step = gp->real_size();
		gp->~Generic();
		mvpt += step;
	}
}

/*
sz must be computed using the exact same
computation used in real_size() of the
object to be allocated.  This includes
alignment.
*/
void* Semispace::alloc(size_t sz) {
	prev_alloc = sz;
	void* tmp = allocpt;
	char* callocpt = allocpt;
	callocpt += sz;
	allocpt = callocpt;
	return tmp;
}

/*should be used only for most recent allocation
(i.e. should be used for freeing memory when the
constructor throws.)
*/
void Semispace::dealloc(void* pt) {
	#ifdef DEBUG
		char* callocpt = allocpt;
		callocpt -= prev_alloc;
		if(callocpt != pt) throw_DeallocError(pt);
	#endif
	allocpt = pt;
}

void* Semispace::lifo_alloc(size_t sz) {
	prevalloc = sz;
	char* clifoallocpt = lifoallocpt;
	clifoallocpt -= sz;
	lifoallocpt = clifoallocpt;
	return lifoallocpt;
}

void Semispace::lifo_dealloc(void* pt) {
	/*if we can't deallocate, just ignore*/
	if(pt != lifoallocpt) return;
	size_t sz = ((Generic*) pt)->real_size();
	((Generic*) pt)->~Generic();
	char* clifoallocpt = pt;
	clifoallocpt += sz;
	lifoallocpt = clifoallocpt;
}

/*This function should be used only when the
constructor for the object fails.  It does
*not* properly destroy the object.
*/
void Semispace::lifo_dealloc_abort(void* pt) {
	#ifdef DEBUG
		if(pt != lifoallocpt) throw_DeallocError(pt);
	#endif
	char* clifoallocpt = lifoallocpt;
	clifoallocpt += prev_alloc;
	lifoallocpt = clifoallocpt;
}

