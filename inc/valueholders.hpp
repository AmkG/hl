#ifndef VALUEHOLDERS_H
#define VALUEHOLDERS_H

/*holds a value, for use with:
1.  Sending a message
2.  Holding the Semispaces for messages
    that have been received
3.  Saving to/from a global variable
*/

#include"objects.hpp"
#include"heaps.hpp"

#include<boost/scoped_ptr.hpp>

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

#endif // VALUEHOLDERS_H
