#ifndef LOCKEDS_H
#define LOCKEDS_H

#include"heaps.hpp"
#include"mutexes.hpp"

#include<boost/noncopyable.hpp>

/*-----------------------------------------------------------------------------
Atomic counter
-----------------------------------------------------------------------------*/
/*
in the future, we may change this class to use some kind
of atomic operations library such as GLib's.
*/
class AtomicCounter : boost::noncopyable {
private:
	size_t num;
	AppMutex m;
public:
	AtomicCounter(size_t nnum = 0) : num(nnum) { }
	/*postfix ++*/
	/*return value:
		0 = the counter was zero BEFORE incrementing
		non-zero = the counter was some non-zero value (which
			might not be equal to the return value)
	*/
	size_t operator++(int) { // int means postfix
		AppLock l(m);
		return num++;
	}
	/*prefix --*/
	/*return value:
		0 = the counter is now zero AFTER decrementing
		non-zero = the counter is now some non-zero value
			(which might not be equal to the return
			value)
	*/
	size_t operator--(void) { // void means prefix
		AppLock l(m);
		return --num;
	}
};

/*-----------------------------------------------------------------------------
Atomic Pointer to ValueHolder
-----------------------------------------------------------------------------*/
/*In the future, this pointer may be implemented with an
atomic operations library, possibly one with CAS.
Note however that this may be susceptible to ABA, since
deletion of unused ValueHolder's is immediate, not
deferred. T.T
*/

class LockedValueHolderRef : boost::noncopyable {
private:
	ValueHolder* p;
	AppMutex m;

public:
	/*we can only insert items or remove items, or swap the contents
	with a ValueHolderRef.
	*/
	void insert(ValueHolderRef& o) {
		#ifdef DEBUG
			if(!o.p) {
				throw_ValueHolderLinkingError(o.p);
			}
			if(o.p->next.p) {
				throw_ValueHolderLinkingError(o.p);
			}
		#endif
		{AppLock l(m);
			o.p->next.p = p;
			p = o.p;
			o.p = 0; /*we've taken responsibility*/
		}
	}
	void remove(ValueHolderRef& o) {
		#ifdef DEBUG
			if(o.p) {
				throw_ValueHolderLinkingError(o.p);
			}
		#endif
		{AppLock l(m);
			if(!p) return;
			o.p = p;
			p = o.p->next.p;
			o.p->next.p = 0;
		}
	}
	void swap(ValueHolderRef& o) {
		AppLock l(m);
		ValueHolder* tmp = p;
		p = o.p;
		o.p = tmp;
	}
};


#endif // LOCKEDS_H

