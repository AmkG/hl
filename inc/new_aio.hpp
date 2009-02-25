#ifndef NEW_AIO_H
#define NEW_AIO_H

/*Defines virtual AIO objects to be instantiated
by concrete implementation in the src/ file.
*/

#include<string>
#include<stdexcept>

#include<boost/shared_ptr.hpp>

class Process;
class Symbol;

/*
The "new" AIO is based on event objects.  Event
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

class Event {
public:

	virtual ~Event() { }
};

boost::shared_ptr<Event> system_event(boost::shared_ptr<ProcessInvoker>,std::string);
boost::shared_ptr<Event> sleep_event(boost::shared_ptr<ProcessInvoker>,size_t);

/*used as a Singleton, although we don't enforce it*/
class EventSetImpl;
class EventSet {
private:
	EventSetImpl* pimpl;

public:
	void add_event(boost::shared_ptr<Event>);
	void remove_event(boost::shared_ptr<Event>);

	EventSet(void);
	~EventSet();
};

/*-----------------------------------------------------------------------------
Shared across Implementations
-----------------------------------------------------------------------------*/

class ProcessInvoker {
private:
	Process* P;
public:
	void read_respond(
		boost::shared_ptr<IOPort>, std::vector<unsigned char> const&
	);
	void write_respond(
		boost::shared_ptr<IOPort>, std::vector<unsigned char> const&
	);
	void accept_respond(
		boost::shared_ptr<IOPort> socket, boost::shared_ptr<IOPort> new_socket
	);
	void sleep_respond(
		boost::shared_ptr<Event>, size_t
	);
	void system_respond(
		boost::shared_ptr<Event>, int term_code
	);
};


#endif // NEW_AIO_H

