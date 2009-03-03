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
4.  Sleep Event
5.  System Event (i.e. spawn child OS process)
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
};
/*
Errors are caught only on:
1. opening a file
2. seeking
3. closing

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
	void close(void);

	virtual boost::shared_ptr<Event> read(
		boost::shared_ptr<ProcessInvoker>, size_t
	) =0;
	virtual boost::shared_ptr<Event> write(boost::shared_ptr<ProcessInvoker>,
		std::vector<unsigned char> const&
	) =0;
	virtual boost::shared_ptr<Event> accept(
		boost::shared_ptr<ProcessInvoker>
	) =0;

	virtual ~IOPort() { }
};

/*Will only be called once, at init*/
boost::shared_ptr<IOPort> ioport_stdin(void);
boost::shared_ptr<IOPort> ioport_stdout(void);
boost::shared_ptr<IOPort> ioport_stderr(void);

boost::shared_ptr<IOPort> infile(std::string);
boost::shared_ptr<IOPort> outfile(std::string);
boost::shared_ptr<IOPort> appendfile(std::string);

void close(boost::shared_ptr<IOPort>);

class Event {
public:
	virtual void seek(uint64_t) =0;

	virtual ~Event() { }
};

boost::shared_ptr<Event> system_event(boost::shared_ptr<ProcessInvoker>,std::string);
/*sleep unit is milliseconds*/
boost::shared_ptr<Event> sleep_event(boost::shared_ptr<ProcessInvoker>,size_t);
/*connect to host's port*/
boost::shared_ptr<Event> connect_event(boost::shared_ptr<ProcessInvoker>,std::string,int);

/*used as a Singleton, although we don't enforce it*/
/*PROMISE: We will only ever create one instance
of this object, and it will only be created in the
main program thread at initialization (note however
that other threads may have been launched; any
other launched threads, if present at the time this
object is instantiated, will not be worker threads).
This means that none of the other functions here,
and none of the objects, will be instantiated before
this object is instantiated.
*/
class EventSetImpl;
class EventSet {
private:
	EventSetImpl* pimpl;

public:
	void add_event(boost::shared_ptr<Event>);
	void remove_event(boost::shared_ptr<Event>);

	/*"host" is the process which performed the
 	'event-wait or 'event-poll bytecode.
 	*/
	void event_poll(Process& host);
	void event_wait(Process& host);

	/*Since this object is only instantiated once, at
	initialization (and we are *assured* of it being
	instantiated), it is appropriate to use the
	constructor for any OS-specific initialization.
	*/
	EventSet(void);
	~EventSet();
};

/*called at the initialization/cleanup of each thread*/
/*NOT called on main process thread*/
void aio_thread_initialize(void);
void aio_thread_deinitialize(void);

/*-----------------------------------------------------------------------------
Shared across Implementations
-----------------------------------------------------------------------------*/

/*implementations in src/aio.cpp*/

class ProcessInvoker {
private:
	Process* P;
	ProcessInvoker(void); //disallowed

public:
	/*"host" parameter is just a heap that can be used to
	conveniently construct the response before actually
	sending it to the target process.  It is *not* the
	target process: it's just used as a scratch heap.
	Reusing an existing process (specifically the process
	that called the event waiting/polling) reduces
	allocation overhead.
	*/
	void io_respond(
		Process& host,
		boost::shared_ptr<IOPort>,
		boost::shared_ptr<std::vector<unsigned char> >&
	);
	void nil_respond(
		Process& host,
		boost::shared_ptr<IOPort>
	);
	void accept_respond(
		Process& host,
		boost::shared_ptr<IOPort> socket,
		boost::shared_ptr<IOPort> new_socket
	);
	void connect_respond(
		Process& host,
		boost::shared_ptr<Event>, boost::shared_ptr<IOPort> new_socket
	);
	void sleep_respond(
		Process& host,
		boost::shared_ptr<Event>, size_t
	);
	void system_respond(
		Process& host,
		boost::shared_ptr<Event>, int term_code
	);

	/*call for errors*/
	void io_error_respond(
		Process& host,
		boost::shared_ptr<IOPort>, std::string const&
	);
	void other_error_respond(
		Process& host,
		boost::shared_ptr<Event>, std::string const&
	);

	explicit ProcessInvoker(Process*);
	~ProcessInvoker(void);

	/*TODO: integrate with workers.cpp so that existing ProcessInvoker's
	are part of the root set.
	*/
};


#endif // NEW_AIO_H

