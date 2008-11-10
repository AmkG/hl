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
		char* callocpt = (char*)allocpt;
		callocpt += Object::alignment - (tmp & Object::tag_mask);
		allocpt = callocpt;
	}
	allocstart = allocpt;

	char* cmem = (char*)mem;
	char* clifoallocpt = cmem + nsz;
	// adjust for alignment
	tmp = reinterpret_cast<intptr_t>(lifoallocpt);
	clifoallocpt -= (tmp & Object::tag_mask);

	lifoallocpt = clifoallocpt;
	lifoallocstart = lifoallocpt;
}

Semispace::~Semispace() {
	Generic* gp;
	size_t step;
	char* mvpt;
	char* endpt;

	/*TODO: consider refactoring*/
	/*----Delete normally-allocated memory----*/
	mvpt = (char*) allocstart;
	endpt = (char*) allocpt;

	while(mvpt < endpt) {
		gp = (Generic*)(void*) mvpt;
		step = gp->real_size();
		gp->~Generic();
		mvpt += step;
	}

	/*----Delete lifo-allocated memory----*/
	mvpt = (char*) lifoallocpt;
	endpt = (char*) lifoallocstart;

	while(mvpt < endpt) {
		gp = (Generic*)(void*) mvpt;
		step = gp->real_size();
		gp->~Generic();
		mvpt += step;
	}

	free(mem);
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
	char* callocpt = (char*) allocpt;
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
		char* callocpt = (char*) allocpt;
		callocpt -= prev_alloc;
		if(callocpt != pt) throw_DeallocError(pt);
	#endif
	allocpt = pt;
}

void* Semispace::lifo_alloc(size_t sz) {
	prev_alloc = sz;
	char* clifoallocpt = (char*) lifoallocpt;
	clifoallocpt -= sz;
	lifoallocpt = clifoallocpt;
	return lifoallocpt;
}

void Semispace::lifo_dealloc(void* pt) {
	/*if we can't deallocate, just ignore*/
	if(pt != lifoallocpt) return;
	size_t sz = ((Generic*) pt)->real_size();
	((Generic*) pt)->~Generic();
	char* clifoallocpt = (char*) pt;
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
	char* clifoallocpt = (char*) lifoallocpt;
	clifoallocpt += prev_alloc;
	lifoallocpt = clifoallocpt;
}

void Semispace::clone(boost::scoped_ptr<Semispace>& sp, Generic*& g) const {
	/*TODO*/
}

/*-----------------------------------------------------------------------------
Heaps
-----------------------------------------------------------------------------*/

/*copy and modify GC class*/
class GCTraverser : public GenericTraverser {
	Semispace* nsp;
public:
	explicit GCTraverser(Semispace* nnsp) : nsp(nnsp) { }
	void traverse(Object::ref& r) {
		if(is_a<Generic*>(r)) {
			Generic* gp = as_a<Generic*>(r);
			BrokenHeart* bp = dynamic_cast<BrokenHeart*>(gp);
			if(bp) { //broken heart
				r = Object::to_ref(bp->to);
			} else { //unbroken
				Generic* ngp = gp->clone(nsp);
				gp->break_heart(ngp);
				r = Object::to_ref(ngp);
			}
		} else return;
	}
};

void Heap::cheney_collection(Semispace* nsp) {
	GCTraverser gc(nsp);
	/*step 1: initial traverse*/
	scan_root_object(&gc);
	/*step 2: non-root traverse*/
	/*notice that we traverse the new semispace
	this is a two-pointer Cheney collector, with mvpt
	being one pointer and nsp->allocpt the other one
	*/
	char* mvpt = (char*) nsp->allocstart;
	while(mvpt < ((char*) nsp->allocpt)) {
		Generic* gp = (Generic*)(void*) mvpt;
		size_t obsz = gp->real_size();
		gp->traverse_references(&gc);
		mvpt += obsz;
	}
}

#ifdef DEBUG
	#include<iostream>
#endif

void Heap::GC(size_t insurance) {

	#ifdef DEBUG
		std::cout << "GC!" << std::endl;
	#endif

	/*Determine the sizes of all semispaces*/
	size_t total = main->used() + insurance;
	total +=
	(other_spaces) ?		other_spaces->used_total() :
	/*otherwise*/			0 ;

	if(tight) total *= 2;

	/*get a new Semispace*/
	boost::scoped_ptr<Semispace> nsp(new Semispace(total));

	/*traverse*/
	cheney_collection(&*nsp);

	/*replace*/
	main.swap(nsp);
	nsp.reset();
	other_spaces.reset();

	/*determine if resizing is appropriate*/
	if(main->used() + insurance <= total / 4) {
		/*semispace a bit large... make it smaller*/
		nsp.reset(new Semispace(total / 2));
		cheney_collection(&*nsp);
		main.swap(nsp);
		nsp.reset();
	} else if(main->used() + insurance >= (total / 4) * 3) {
		tight = 1;
	}
}


