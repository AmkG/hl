#ifndef OBJ_AIO_H
#define OBJ_AIO_H

#include"types.hpp"
#include"aio.hpp"

#include<string>

inline Object::ref add_event(Object::ref ev) {
	#ifdef DEBUG
		if(!maybe_type<HlEvent>(ev)) {
			throw_HlError("add-event: Expected an event");
		}
	#endif
	HlEvent* ep = known_type<HlEvent>(ev);
	the_event_set().add_event(ep->p);
	return Object::t();
}

inline Object::ref remove_event(Object::ref ev) {
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

inline Object::ref empty_event_set(void) {
	return
		the_event_set().empty() ?		Object::t() :
		/*otherwise*/				Object::nil() ;
}

inline Object::ref event_poll(Process& P) {
	the_event_set().event_poll(P);
	return Object::t();
}

inline Object::ref event_wait(Process& P) {
	the_event_set().event_wait(P);
	return Object::t();
}

inline Object::ref only_running(Process& P) {
	return
		P.is_only_running() ?			Object::t() :
		/*otherwise*/				Object::nil() ;
}

static inline Object::ref io_accept(Process& host, Object::ref proc, Object::ref port) {
	#ifdef DEBUG
		if(!maybe_type<HlPid>(proc)) {
			throw_HlError("accept: Expected a process");
		}
		if(!maybe_type<HlIOPort>(port)) {
			throw_HlError("accept: Expected an I/O port");
		}
	#endif
	boost::shared_ptr<IOPort> now_accept;
	boost::shared_ptr<ProcessInvoker> pi(
		new ProcessInvoker(
			known_type<HlPid>(proc)->process
		)
	);
	try {
		boost::shared_ptr<Event> event =
			known_type<HlIOPort>(port)->p->accept(pi, now_accept)
		;
		if(event) {
			HlEvent* hp = host.create<HlEvent>();
			hp->hl_pid = proc;
			hp->p = event;
			return Object::to_ref<Generic*>(hp);
		} else {
			#ifdef DEBUG
				if(!now_accept) {
					throw_HlError("accept: did not "
						"return an event, a port, "
						"or an error"
					);
				}
			#endif
			HlIOPort* hp = host.create<HlIOPort>();
			hp->p = now_accept;
			return Object::to_ref<Generic*>(hp);
		}
	} catch(IOError& err) {
		std::string errmsg = err.what();
		/*for now, assume error messages are purely ASCII*/
		for(size_t i = 0; i < errmsg.size(); ++i) {
			host.stack.push(Object::to_ref(UnicodeChar(errmsg[i])));
		}
		HlString::stack_create(host, host.stack, errmsg.size());
		HlTagged* hp = host.create<HlTagged>();
		hp->o_type = symbol_io;
		hp->o_rep = stack.top(); stack.pop();
		return Object::to_ref<Generic*>(hp);
		/*TODO*/
	}
}

#endif // OBJ_AIO_H

