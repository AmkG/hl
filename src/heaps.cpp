#include"all_defines.hpp"
#include"objects.hpp"
#include"heaps.hpp"
#include"types.hpp"

#include<map>
#include<stack>
#include<cstdlib>
#include<stdint.h>

/*-----------------------------------------------------------------------------
Semispaces
-----------------------------------------------------------------------------*/

Semispace::Semispace(size_t nsz) {
	nsz = Object::round_up_to_alignment(nsz);
	max = nsz;
	// add alignment in case mem is misaligned
	mem = std::malloc(nsz + Object::alignment);
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
	tmp = reinterpret_cast<intptr_t>(clifoallocpt);
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

	std::free(mem);
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

void Semispace::lifo_dealloc(Generic* pt) {
	/*if we can't deallocate, just ignore*/
	if(((void*) pt) != lifoallocpt) return;
	size_t sz = pt->real_size();
	pt->~Generic();
	char* clifoallocpt = (char*)(void*) pt;
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

void Semispace::traverse_objects(HeapTraverser* ht) const {
	char* mvpt = (char*) allocstart;
	char* endpt = (char*) allocpt;
	while(mvpt < endpt) {
		Generic* tmp = (Generic*)(void*) mvpt;
		size_t sz = tmp->real_size();
		ht->traverse(tmp);
		mvpt += sz;
	}
}

/*Used by SemispaceCloningTraverser below*/
class MovingTraverser : public GenericTraverser {
private:
	ptrdiff_t diff;
public:
	void traverse(Object::ref& o) {
		if(is_a<Generic*>(o)) {
			Generic* gp = as_a<Generic*>(o);
			char* cgp = (char*)(void*) gp;
			cgp -= diff;
			gp = (Generic*)(void*) cgp;
			o = Object::to_ref(gp);
		}
	}
	explicit MovingTraverser(ptrdiff_t ndiff) : diff(ndiff) { }
};

class SemispaceCloningTraverser : public HeapTraverser {
private:
	Semispace* sp;
	MovingTraverser mt;
public:
	void traverse(Generic* gp) {
		Generic* np = gp->clone(sp);
		np->traverse_references(&mt);
	}
	SemispaceCloningTraverser(Semispace* nsp, ptrdiff_t ndiff)
		: sp(nsp), mt(ndiff) { }
};

/*Preconditions:
	this should be self-contained (i.e. objects in it
	  should not contain references to objects outside
	  of this semispace)
	this should have no lifo-allocated objects
*/
void Semispace::clone(boost::scoped_ptr<Semispace>& ns, Generic*& g) const {
	ns.reset(new Semispace(max));
	char* myallocstart = (char*) allocstart;
	char* hisallocstart = (char*) ns->allocstart;

	SemispaceCloningTraverser sct(&*ns, myallocstart - hisallocstart);

	traverse_objects(&sct);

	char* cg = (char*)(void*) g;
	cg -= (myallocstart - hisallocstart);
	g = (Generic*)(void*) cg;
}

/*-----------------------------------------------------------------------------
ValueHolder
-----------------------------------------------------------------------------*/

void ValueHolder::traverse_objects(HeapTraverser* ht) const {
	for(ValueHolder const* pt = this; pt; pt = &*pt->next) {
		if(pt->sp) pt->sp->traverse_objects(ht);
	}
}

/*clones only itself, not the chain*/
void ValueHolder::clone(ValueHolderRef& np) const {
	np.p = new ValueHolder();
	if(sp) {
		boost::scoped_ptr<Semispace> nsp;
		Generic* gp = as_a<Generic*>(val);
		sp->clone(nsp, gp);
		np->sp.swap(nsp);
		np->val = Object::to_ref(gp);
	} else {
		np->val = val;
	}
}

class ObjectMeasurer : public GenericTraverser {
public:
	size_t N;
	std::map<Generic*,Generic*>* mp;
	std::stack<Generic*> todo;

	explicit ObjectMeasurer(std::map<Generic*,Generic*>& m)
		: N(0), mp(&m) { }

	void traverse(Object::ref& o) {
		if(is_a<Generic*>(o)) {
			Generic* gp = as_a<Generic*>(o);
			if(mp->find(gp) == mp->end()) {
				N += gp->real_size();
				(*mp)[gp] = gp;
				todo.push(gp);
			}
		}
	}
	void operate(Generic* gp) {
		(*mp)[gp] = gp;
		todo.push(gp);
		do {
			gp = todo.top(); todo.pop();
			gp->traverse_references(this);
		} while(!todo.empty());
	}
};

class ReferenceReplacer : public GenericTraverser {
private:
	std::map<Generic*, Generic*>* mp;
public:
	explicit ReferenceReplacer(std::map<Generic*, Generic*>& m)
		: mp(&m) { }
	void traverse(Object::ref& o) {
		if(is_a<Generic*>(o)) {
			std::map<Generic*, Generic*>::iterator it;
			it = mp->find(as_a<Generic*>(o));
			if(it != mp->end()) {
				o = Object::to_ref(it->second);
			}
		}
	}
};

void ValueHolder::copy_object(ValueHolderRef& np, Object::ref o) {
	typedef std::map<Generic*, Generic*> TM;
	if(is_a<Generic*>(o)) {
		TM obs;
		size_t total;
		/*first, measure the memory*/
		{ObjectMeasurer om(obs);
			om.operate(as_a<Generic*>(o));
			total = om.N;
		}
		/*now create the Semispace*/
		boost::scoped_ptr<Semispace> sp(new Semispace(total));

		/*copy*/
		for(TM::iterator it = obs.begin(); it != obs.end(); ++it) {
			it->second = it->second->clone(&*sp);
		}

		/*translate*/
		{ObjectsTraverser<ReferenceReplacer> ot(obs);
			sp->traverse_objects(&ot);
		}
		o = Object::to_ref(obs[as_a<Generic*>(o)]);

		/*create holder*/
		np.p = new ValueHolder;
		np.p->val = o;
		np.p->sp.swap(sp);
	} else {
		np.p = new ValueHolder;
		np.p->val = o;
	}
}

/*-----------------------------------------------------------------------------
Heaps
-----------------------------------------------------------------------------*/

void Heap::traverse_objects(HeapTraverser* ht) const {
	main->traverse_objects(ht);
	if(!other_spaces.empty()) {
		other_spaces->traverse_objects(ht);
	}
}

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
	(other_spaces.empty()) ?	0 :
	/*otherwise*/			other_spaces->used_total() ;

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


