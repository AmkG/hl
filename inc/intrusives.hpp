#ifndef INTRUSIVES_H
#define INTRUSIVES_H

#include"lockeds.hpp"

#include<boost/intrusive_ptr.hpp>

class WithRefCount;

void intrusive_ptr_add_ref(WithRefCount*);
void intrusive_ptr_release(WithRefCount*);

class WithRefCount {
protected:
	AtomicCounter ref_count;
public:
	friend void intrusive_ptr_add_ref(WithRefCount*);
	friend void intrusive_ptr_release(WithRefCount*);
};

static inline void intrusive_ptr_add_ref(WithRefCount* o) {
	o->ref_count++;
}
static inline void intrusive_ptr_release(WithRefCount* o) {
	if((--o->ref_count) == 0) delete o;
}

#endif // INTRUSIVES_H

