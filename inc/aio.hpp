#ifndef NEW_AIO_H
#define NEW_AIO_H

/*Defines virtual AIO objects to be instantiated
by concrete implementation in the src/ file.
*/

#include<string>
#include<vector>
#include<stdexcept>

#include<stdint.h>

#include<boost/shared_ptr.hpp>

class Process;
class Symbol;

/*
The AIO is based on event objects.  Event
objects are just passive requests; in order to
"arm" an event, it should be added to the global
event waiting set object.

Events are:
1.  Write-to-I/O event
2.  Read-from-I/O event
3.  Accept-from-I/O Socket event
4.  Connect-to-host-socket event
5.  Sleep Event
6.  System Event (i.e. spawn child OS process)
*/

/*Details are handled by the AIO implementation
This include file deals only in abstractions;
we don't care how they are actually implemented.
*/

class Event;
class ProcessInvoker;

/*Errors in I/O should throw this class*/
/*Could use a Maybe T type I suppose though*/
class IOError : public std::runtime_error {
public:
	IOError(std::string const& a) : std::runtime_error(a) { }
};
/*
Errors are caught only on:
1. opening a file
2. seeking
3. closing
4. creating a read event
5. creating a write event
6. creating an accept event

In all other cases, errors are caught at the time
that events are triggered; they are reported by
sending messages to the process being informed.
*/

/*-----------------------------------------------------------------------------
Implementation-specific
-----------------------------------------------------------------------------*/

/*I/O Ports*/
class IOPort {
public:
	virtual void close(void) =0;

	/*If the read completed immediately, return a null
	pointer and put a value into now_read.  Otherwise,
	put a null pointer into now_read and return an
	event.
	*/
	virtual boost::shared_ptr<Event> read(
		boost::shared_ptr<ProcessInvoker> const&, size_t,
		boost::shared_ptr<std::vector<unsigned char> >& now_read
	) =0;

	/*return a null pointer if data-writing was immediately
	completed.
	*/
	virtual boost::shared_ptr<Event> write(
		boost::shared_ptr<ProcessInvoker>,
		boost::shared_ptr<std::vector<unsigned char> >&
	) =0;

	/*If the accept completed immediately, return a
	null pointer and put a value into now_accept.
	Otherwise, put a null pointer into now_accept and
	return an event.
	*/
	virtual boost::shared_ptr<Event> accept(
		boost::shared_ptr<ProcessInvoker>,
		boost::shared_ptr<IOPort>& now_accept
	) =0;

	/*return a null pointer if fsync was immediately
	completed.
	*/
	virtual boost::shared_ptr<Event> fsync(
		boost::shared_ptr<ProcessInvoker>
	) =0;

	virtual void seek(uint64_t) =0;
	virtual uint64_t tell(void) =0;

	virtual ~IOPort() { }
};

/*Will only be called once, at init*/
boost::shared_ptr<IOPort> ioport_stdin(void);
boost::shared_ptr<IOPort> ioport_stdout(void);
boost::shared_ptr<IOPort> ioport_stderr(void);
/*PROMISE: we won't call the above functions unless
we have called aio_initialize() once, first.
PROMISE: the above functions are only called once,
during initialization.
*/

boost::shared_ptr<Event> infile(std::string, boost::shared_ptr<IOPort>&);
boost::shared_ptr<Event> outfile(std::string, boost::shared_ptr<IOPort>&);
boost::shared_ptr<Event> appendfile(std::string, boost::shared_ptr<IOPort>&);

class Event {
public:
	virtual ~Event() { }
};

boost::shared_ptr<Event> system_event(boost::shared_ptr<ProcessInvoker>,std::string);
/*sleep unit is milliseconds*/
boost::shared_ptr<Event> sleep_event(boost::shared_ptr<ProcessInvoker>,size_t);
/*connect to host's port*/
boost::shared_ptr<Event> connect_event(boost::shared_ptr<ProcessInvoker>,std::string,int);

class ProcessInvokerScanner;

/*used as a Singleton, although we don't enforce it*/
/*specific AIO implementation has to enforce Singleton-ness*/
class EventSetImpl;
class EventSet {
private:
	EventSetImpl* pimpl;

public:
	/*return true if empty*/
	bool empty(void) const;

	void add_event(boost::shared_ptr<Event>);
	bool remove_event(boost::shared_ptr<Event>);

	/*"host" is the process which performed the
 	'event-wait or 'event-poll bytecode.
 	*/
	void event_poll(Process& host);
	void event_wait(Process& host);

	EventSet(void);
	~EventSet();

	/*This member function is called on the global
	event set by the thread pool.  This function
	should then call the traverse() member function
	of the given ProcessInvokerScanner on each
	process invoker of each live event.
	This call is assuredly made when only one thread
	is running, and thus does not require any special
	threading protection.
	*/
	void scan_process_invokers(ProcessInvokerScanner*);
};

/*get *the* EventSet*/
EventSet& the_event_set(void);
/*PROMISE: we won't call the above function unless
we have called aio_initialize() below, once, first.
*/
/*initialize and clean-up aio (including creation of *the* EventSet)*/
void aio_initialize(void);
void aio_deinitialize(void);

/*called at the initialization/cleanup of each thread*/
/*NOT called on main process thread*/
void aio_thread_initialize(void);
void aio_thread_deinitialize(void);

/*-----------------------------------------------------------------------------
Shared across Implementations
-----------------------------------------------------------------------------*/

class ProcessInvokerScanner {
public:
	virtual void traverse(ProcessInvoker const&) =0;
	virtual ~ProcessInvokerScanner() { }
};

/*implementations in src/aio.cpp*/

class ProcessInvoker {
private:
	ProcessInvoker(void); //disallowed

public:
	Process* P;

	/*"host" parameter is just a heap that can be used to
	conveniently construct the response before actually
	sending it to the target process.  It is *not* the
	target process: it's just used as a scratch heap.
	Reusing an existing process (specifically the process
	that called the event waiting/polling) reduces
	allocation overhead.
	NOTE: these functions *must* be called *before* the
	event is removed from the event set.
	*/
	void io_respond(
		Process& host,
		boost::shared_ptr<Event>&,
		boost::shared_ptr<std::vector<unsigned char> >&
	);
	void nil_respond(
		Process& host,
		boost::shared_ptr<Event>&
	);
	void accept_respond(
		Process& host,
		boost::shared_ptr<Event>&,
		boost::shared_ptr<IOPort>& new_socket
	);
	void connect_respond(
		Process& host,
		boost::shared_ptr<Event>&, boost::shared_ptr<IOPort>& new_socket
	);
	void sleep_respond(
		Process& host,
		boost::shared_ptr<Event>&, size_t
	);
	void system_respond(
		Process& host,
		boost::shared_ptr<Event>&, int term_code
	);

	/*call for errors*/
	void error_respond(
		Process& host,
		boost::shared_ptr<Event>&, std::string const&
	);

	explicit ProcessInvoker(Process*);
	~ProcessInvoker(void);

};


#endif // NEW_AIO_H

