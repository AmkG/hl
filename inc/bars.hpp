#ifndef BARS_H
#define BARS_H

/*
 * Defines memory access barriers to assist generational GC
 */

#include"workarounds.hpp"
#include"generics.hpp"

namespace Object {
	class bar_ref;
};

/*
The current implementation uses a structure called a
Sequential Store Buffer or SSB to keep track of any
potentially intergenerational pointers.
The SSB is just a large array indexed into by a
pointer (here, __ssb_p).  Each write not only performs
the actual write to the object slot, it also writes
the object slot's address to the SSB, then increments
the __ssb_p pointer.
The SSB is always initialized with NULL values; we
expect that any object slots will have non-NULL
addresses.  The SSB is terminated by a non-NULL
pointer, which is actually a pointer to the heap
that the SSB is attached to.
Access to the SSB is acquired by the macro
`WRITE_BARRIER_ACQUIRE(heap)`, where heap is a
reference to the heap whose objects you want to
mutate.  Only one write barrier acquisition should
be active at one time:
	// WRONG!
	WRITE_BARRIER_ACQUIRE(*hp);
	do_stuff(WRITE_BARRIER);
	{Mother your_mom;
		WRITE_BARRIER_ACQUIRE(*hp);
	}
The right thing to do would be:
	{WRITE_BARRIER_ACQUIRE(*hp);
		do_stuff(WRITE_BARRIER);
	}
	{Mother your_mom;
		WRITE_BARRIER_ACQUIRE(*hp);
	}
*/

#ifndef ONLY_COPYING_GC
	Object::ref** __ssb_clean(Object::ref**);
	#define WRITE_BARRIER , __ssb_p
	#define WRITE_BARRIER_DECL , Object::ref**& __ssb_p
	#define WRITE_BARRIER_ACQUIRE(P)\
		__SSBProtector __ssbPP((P)); \
		Object::ref**& __ssb_p = __ssbPP.pt
#else
	#define WRITE_BARRIER
	#define WRITE_BARRIER_DECL
	#define WRITE_BARRIER_ACQUIRE(P)
#endif

namespace Object {

	class bar_ref {
	private:
		Object::ref me;
	public:
		/*Usage:
		class Mom : GenericDerived<Mom> {
		private:
			Object::bar_ref life;
		public:
			Object::ref insert(Object::ref nval
					WRITE_BARRIER_DECL) {
				life.write(nval WRITE_BARRIER);
			}
		};
		Mom& your_mom = *hp.create<Mom>();
		your_mom.insert(half_my_source_code WRITE_BARRIER);
		*/
		inline Object::ref write(Object::ref val WRITE_BARRIER_DECL)
				F_INLINE {
			me = val;
			#ifndef ONLY_COPYING_GC
				/*
				When JITting with lightning:
					lightning exposes 3 callee-saved
					registers on all platforms: V0,
					V1, and V2.
					Two of these can be used for the
					stack top and stack bottom.
					The last can be used for __ssb_p
				*/
				/*write barrier*/
				/*__ssb_p is NULL if no barrier is
				necessary
				*/
				if(__ssb_p) { //assume equal likelihood
					*(__ssb_p++) = &me;
					/*End of SSB is a non-NULL pointer
					(This pointer is actually a pointer
					to the heap associated with the SSB)
					*/
					/*SSB is pretty big, so this will
					rarely trigger
					*/
					if(unlikely(*__ssb_p)) {
						__ssb_p = __ssb_clean(
							__ssb_p
						);
					}
				}
			#endif
		}
		inline Object::ref init(Object::ref val) F_INLINE {
			me = val;
		}
		inline operator Object::ref(void) const F_INLINE {
			return me;
		}
		inline void traverse_references(GenericTraverser* gt) {
			gt->traverse(me);
		}
	};
};

#ifndef ONLY_COPYING_GC
	/*class to update ssb pointer*/
	class __SSBProtector : boost::noncopyable {
	private:
		Heap* hp;
	public:
		Object::ref** pt;
		explicit __SSBProtector(Heap& heap)
			: hp(&heap), pt(heap.acquire_ssb()) { }
		~__SSBProtector() {
			hp->release_ssb(pt);
		}
	};
#endif

#endif // BARS_H

