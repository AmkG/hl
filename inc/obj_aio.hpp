#ifndef OBJ_AIO_H
#define OBJ_AIO_H

#include"types.hpp"
#include"aio.hpp"
#include"bytecodes.hpp"

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

static inline boost::shared_ptr<ProcessInvoker> create_process_invoker(
		Object::ref proc ) {
	#ifdef DEBUG
		if(!maybe_type<HlPid>(proc)) {
			throw_HlError("I/O: Expected a process");
		}
	#endif
	return boost::shared_ptr<ProcessInvoker>(
		new ProcessInvoker(
			known_type<HlPid>(proc)->process
		)
	);
}

static inline Object::ref create_event(
		Process& host,
		Object::ref proc,
		boost::shared_ptr<Event>& event) {
	/*prevent the given proc from being lost if a GC triggers*/
	host.stack.push(proc);
	HlEvent* hp = host.create<HlEvent>();
	hp->hl_pid = host.stack.top(); host.stack.pop();
	hp->p = event;
	return Object::to_ref<Generic*>(hp);
}

static inline Object::ref create_io_port(
		Process& host,
		boost::shared_ptr<IOPort>& port) {
	HlIOPort* hp = host.create<HlIOPort>();
	hp->p = port;
	return Object::to_ref<Generic*>(hp);
}

static inline Object::ref handle_io_error(Process& host, IOError& err) {
	std::string errmsg = err.what();
	ProcessStack& stack = host.stack;
	/*for now, assume error messages are purely ASCII*/
	for(size_t i = 0; i < errmsg.size(); ++i) {
		host.stack.push(Object::to_ref(UnicodeChar(errmsg[i])));
	}
	HlString::stack_create(host, host.stack, errmsg.size());
	HlTagged* hp = host.create<HlTagged>();
	hp->o_type = Object::to_ref(symbol_io);
	hp->o_rep = stack.top(); stack.pop();
	return Object::to_ref<Generic*>(hp);
}

/*
NOTE!  Many of the functions below are specially coded to
ensure that they only use their Object::ref arguments
BEFORE they ever allocate anything on the hosting
process.
*/

inline Object::ref io_accept(Process& host, Object::ref proc, Object::ref port) {
	#ifdef DEBUG
		if(!maybe_type<HlIOPort>(port)) {
			throw_HlError("accept: Expected an I/O port");
		}
	#endif
	boost::shared_ptr<IOPort> now_accept;
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event =
			known_type<HlIOPort>(port)->p->accept(pi, now_accept)
		;
		if(event) {
			return create_event(host, proc, event);
		} else {
			#ifdef DEBUG
				if(!now_accept) {
					throw_HlError("accept: did not "
						"return an event, a port, "
						"or an error"
					);
				}
			#endif
			return create_io_port(host, now_accept);
		}
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

template<boost::shared_ptr<Event> (*OF)(boost::shared_ptr<ProcessInvoker>, std::string, boost::shared_ptr<IOPort>& )>
inline Object::ref io_openfile(Process& host, Object::ref proc, Object::ref file) {
	#ifdef DEBUG
		expect_type<HlString>(
			file,
			"open a file: expected a string for filename"
		);
	#endif
	boost::shared_ptr<IOPort> now_port;
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event = (*OF)(
			pi,
			known_type<HlString>(file)->to_cpp_string(),
			now_port
		);
		if(event) {
			return create_event(host, proc, event);
		} else {
			#ifdef DEBUG
				if(!now_port) {
					throw_HlError("open a file: did not "
						"return an event, a port, "
						"or an error"
					);
				}
			#endif
			return create_io_port(host, now_port);
		}
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_connect(Process& host, Object::ref proc, Object::ref target, Object::ref port) {
	#ifdef DEBUG
		expect_type<HlString>(
			target,
			"connect: expected a string for hostname"
		);
		if(!is_a<int>(port)) {
			throw_HlError(
				"connect: expected a number for port number"
			);
		}
	#endif
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event = connect_event(
			pi,
			known_type<HlString>(target)->to_cpp_string(),
			as_a<int>(port)
		);
		#ifdef DEBUG
			if(!event) {
				throw_HlError("'connect did not return an event!");
			}
		#endif
		return create_event(host, proc, event);
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_fsync(Process& host, Object::ref proc, Object::ref port) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"fsync: expected an I/O port for port"
		);
	#endif
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event =
			known_type<HlIOPort>(port)->p->fsync(pi)
		;
		if(event) {
			return create_event(host, proc, event);
		} else {
			return Object::nil();
		}
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_listener(Process& host, Object::ref proc, Object::ref port) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"listener: expected an I/O port for port"
		);
	#endif
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event =
			listener_event(pi, as_a<int>(port))
		;
		#ifdef DEBUG
			if(!event) {
				throw_HlError("'listen did not return an event!");
			}
		#endif
		return create_event(host, proc, event);
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_read(Process& host, Object::ref proc, Object::ref port, Object::ref len) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"read: expected an I/O port for port"
		);
		if(!is_a<int>(len)) {
			throw_HlError(
				"read: expected an integer for length"
			);
		}
	#endif
	boost::shared_ptr<std::vector<unsigned char> > now_read;
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event =
			known_type<HlIOPort>(port)->p->read(
				pi, as_a<int>(len), now_read
			)
		;
		if(event) {
			return create_event(host, proc, event);
		} else {
			#ifdef DEBUG
				if(!now_read) {
					throw_HlError("read: did not "
						"return an event or a "
						"binary blob."
					);
				}
			#endif
			return Object::to_ref<Generic*>(
				BinObj::create(host, now_read)
			);
		}
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_seek(Process& host, Object::ref port, Object::ref point) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"seek: expected an I/O port for port"
		);
		if(point) {
			expect_type<Cons>(
				point,
				"seek: expected a Cons cell set for point"
			);
		}
	#endif
	/*translate the list of numbers into a single, 64-bit number
	e.g:
		(list 1 2 3)
		==>
		1 + 2 * 16777216 + 3 * 281474976710656
	i.e.
	The first number in the list is always the least 24 bits.
	Succeeding numbers in the list are higher-numbered offsets,
	by 24 bits.
	*/
	uint64_t value = 0, off = 1;
	Object::ref p1;
	p1 = point;
	while(p1) {
		#ifdef DEBUG
			expect_type<Cons>(
				p1,
				"seek: not a complete Cons cell"
			);
		#endif
		Object::ref v = car(p1);
		#ifdef DEBUG
			if(!is_a<int>(v)) {
				throw_HlError(
					"expected list of integers "
					"in seek point"
				);
			}
		#endif
		value += off * ((unsigned int)as_a<int>(v));
		off = off << 24;
		if(off == 0) break;
		p1 = cdr(p1);
	}
	try {
		known_type<HlIOPort>(port)->p->seek(
			value
		);
		return Object::t();
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

template<boost::shared_ptr<IOPort>* pp>
inline Object::ref io_builtin_port(Process& proc) {
	boost::shared_ptr<IOPort>& port = *pp;
	HlIOPort* hp = proc.create<HlIOPort>();
	hp->p = port;
	return Object::to_ref<Generic*>(hp);
}

inline Object::ref io_tell(Process& host, Object::ref port) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"tell: expected an I/O port for port"
		);
	#endif
        uint64_t value;
	try {
		value = known_type<HlIOPort>(port)->p->tell();
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
	/*now construct a bunch of Cons cells, divided into 24-bit values*/
	uint64_t sub;
	size_t times = 0;
	ProcessStack& stack = host.stack;
	/*first push all on stack*/
	while(value > 0) {
		sub = value % (((uint64_t) 1) << 24);
		value = value >> 24;
		++times;
		stack.push(Object::to_ref<int>(sub));
	}
	stack.push(Object::nil());
	/*now construct list*/
	for(size_t i = 0; i < times; ++i) {
		/*never refer to port after this point*/
		bytecode_cons(host, stack);
	}
	Object::ref rv = stack.top(); stack.pop();
	return rv;
}

inline Object::ref io_write(Process& host, Object::ref proc, Object::ref port, Object::ref dat) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"write: expected an I/O port for port"
		);
		expect_type<BinObj>(
			dat,
			"write: expected a binary object for data"
		);
	#endif
	boost::shared_ptr<ProcessInvoker> pi = create_process_invoker(proc);
	try {
		boost::shared_ptr<Event> event =
			known_type<HlIOPort>(port)->p->write(
				pi,
				known_type<BinObj>(dat)->pdat
			)
		;
		if(event) {
			return create_event(host, proc, event);
		} else {
			return Object::nil();
		}
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

inline Object::ref io_close(Process& host, Object::ref port) {
	#ifdef DEBUG
		expect_type<HlIOPort>(
			port,
			"close: expected an I/O port for port"
		);
	#endif
	try {
		known_type<HlIOPort>(port)->p->close();
		return Object::nil();
	} catch(IOError& err) {
		return handle_io_error(host, err);
	}
}

#endif // OBJ_AIO_H

