#ifndef OBJ_AIO_H
#define OBJ_AIO_H

#include"types.hpp"
#include"aio.hpp"

static inline Object::ref add_event(Object::ref ev) {
	#ifdef DEBUG
		if(!maybe_type<HlEvent>(ev)) {
			throw_HlError("add-event: Expected an event");
		}
	#endif
	HlEvent* ep = known_type<HlEvent>(ev);
	the_event_set().add_event(ep->p);
	return Object::t();
}

static inline Object::ref remove_event(Object::ref ev) {
	#ifdef DEBUG
		if(!maybe_type<HlEvent>(ev)) {
			throw_HlError("remove-event: Expected an event");
		}
	#endif
	HlEvent* ep = known_type<HlEvent>(ev);
	return
		the_event_set().remove_event(ep->p) ?	Object::t() :
		/*otherwise*/				Object::nil() ;
}

static inline Object::ref empty_event_set(void) {
	return
		the_event_set().empty() ?		Object::t() :
		/*otherwise*/				Object::nil() ;
}

static inline Object::ref event_poll(void) {
	the_event_set().event_poll();
	return Object::t();
}

static inline Object::ref event_wait(void) {
	the_event_set().event_wait();
	return Object::t();
}

static inline Object::ref only_running(Process& P) {
	return
		P.is_only_running() ?			Object::t() :
		/*otherwise*/				Object::nil() ;
}

#endif // OBJ_AIO_H

