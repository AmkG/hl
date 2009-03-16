#include"all_defines.hpp"

/*
POSIX-based asynchronous I/O and signal handling

Note that we don't program to the letter of the POSIX
specs: AFAIK no actual OS conforms perfectly to POSIX.
Preferably we try to be somewhat more resilient and
try to be more defensive when making system calls.
*/

#include"aio.hpp"

#include<unistd.h>
#include<errno.h>
#include<signal.h>
#include<fcntl.h>

#include<boost/noncopyable.hpp>
#include<boost/enable_shared_from_this.hpp>

// used for error reporting before aborting
#include<iostream>
#include<set>

static char const* write_error_message(void);
static char const* read_error_message(void);

/*-----------------------------------------------------------------------------
Selection Between select() and poll()
-----------------------------------------------------------------------------*/

/*
Choose either USE_POSIX_POLL or USE_POSIX_SELECT

Generally, we would prefer to use poll() rather
than select()
*/
#ifdef USE_POSIX_POLL
	#ifdef USE_POSIX_SELECT
		#error Please select only one of USE_POSIX_POLL or USE_POSIX_SELECT
	#else
		#include<poll.h>
	#endif
#else
	#ifndef USE_POSIX_SELECT
		#error Please select either USE_POSIX_POLL or USE_POSIX_SELECT
	#else
		#include<sys/select.h>
	#endif
#endif

#ifdef USE_POSIX_POLL
	#error Support for poll() not yet implemented!
#endif

#ifdef USE_POSIX_SELECT
	/*select() has limits on maximum FD it can handle*/
	#define INVALID_FD_VAL(fd) ((fd) >= FD_SETSIZE)
#else // USE_POSIX_POLL
	#define INVALID_FD_VAL(fd) (0)
#endif

/*-----------------------------------------------------------------------------
Concrete IOPort class declarations
-----------------------------------------------------------------------------*/

class WriteEvent;
class ReadEvent;
class AcceptEvent;
class ConnectEvent;
class SleepEvent;
class SystemEvent;

/*Concrete IOPort type*/
class PosixIOPort : public IOPort, boost::noncopyable {
private:
	int fd;
	bool readable;
	bool writeable;
	bool listener;
	bool closed;

	PosixIOPort(void); // disallowed!

	/*Use a factory, not this!*/
	PosixIOPort(int nfd, bool nr, bool nw, bool nl) 
		: fd(nfd),
		readable(nr),
		writeable(nw),
		listener(nl),
		closed(0) { }

public:
	static inline PosixIOPort* r_able(int nfd) {
		return new PosixIOPort(nfd, 1, 0, 0);
	}
	static inline PosixIOPort* w_able(int nfd) {
		return new PosixIOPort(nfd, 0, 1, 0);
	}
	static inline PosixIOPort* rw_able(int nfd) {
		return new PosixIOPort(nfd, 1, 1, 0);
	}

	void close(void) {
		if(!closed) {
			int rv;
			do {
				errno = 0;
				rv = ::close(fd);
			} while(rv < 0 && errno == EINTR);
			closed = 1;
			/*I/O error reported only at close time T.T */
			if(rv < 0) {
				throw IOError(std::string("I/O Error at close"));
			}
		}
	}

	/*TODO: definitions for these*/
	boost::shared_ptr<Event> read(
			boost::shared_ptr<ProcessInvoker> const&,
			size_t,
			boost::shared_ptr<std::vector<unsigned char> >&
	);
	boost::shared_ptr<Event> write(
		boost::shared_ptr<ProcessInvoker>,
		boost::shared_ptr<std::vector<unsigned char> >&
	);
	boost::shared_ptr<Event> accept(
		boost::shared_ptr<ProcessInvoker>
	);

	void seek(uint64_t);

	/*on dtor, try to close anyway*/
	~PosixIOPort() {
		if(!closed) {
			int rv;
			do {
				errno = 0;
				rv = ::close(fd);
			} while(rv < 0 && errno == EINTR);
			/*ignore errors*/
		}
	}

	friend class WriteEvent;
	friend class ReadEvent;
	friend class AcceptEvent;
};

/*-----------------------------------------------------------------------------
Concrete Event class declarations
-----------------------------------------------------------------------------*/

class IOEvent : public Event, public boost::enable_shared_from_this<IOEvent> {
protected:
	/*These are included here for reuse of send_*() member function*/
	boost::shared_ptr<ProcessInvoker> proc;

	explicit IOEvent(
		boost::shared_ptr<ProcessInvoker> const& nproc
	) : proc(nproc) { }

	void send_error(Process& host, char const* s) {
		boost::shared_ptr<Event> tmp(
			boost::static_pointer_cast<Event>(shared_from_this())
		);
		std::string tmps(s);
		proc->error_respond(
			host,
			tmp,
			tmps
		);
	}

	void send_nil(Process& host) {
		boost::shared_ptr<Event> tmp(
			boost::static_pointer_cast<Event>(shared_from_this())
		);
		proc->nil_respond(host, tmp);
	}

	void send_data(
			Process& host,
			boost::shared_ptr<std::vector<unsigned char> >&
				pdat) {
		boost::shared_ptr<Event> tmp(
			boost::static_pointer_cast<Event>(shared_from_this())
		);
		proc->io_respond(host, tmp, pdat);
	}

public:
	#ifdef USE_POSIX_SELECT
		virtual void add_select_event(
			fd_set& rd, fd_set& wr, fd_set& ex
		) const =0;
		virtual void remove_select_event(
			fd_set& rd, fd_set& wr, fd_set& ex
		) const =0;
		/*return true if detected in select event*/
		virtual bool is_in_select_event(
			fd_set& rd, fd_set& wr, fd_set& ex
		) const =0;
		virtual int get_fd(void) const =0;
	#endif

	/*return true if event completed successfully and
	we can remove the event from the event set.
	false means we haven't completed yet.
	*/
	virtual bool perform_event(Process& host) =0;

	void scan_process_invokers(ProcessInvokerScanner* pis) {
		pis->traverse(*proc);
	}
};

class WriteEvent : public IOEvent {
private:
	int fd;
	boost::shared_ptr<std::vector<unsigned char> > dat;
	size_t start_write;

	WriteEvent(void); // disallowed!
	WriteEvent(
		boost::shared_ptr<ProcessInvoker> const& nproc,
		PosixIOPort const& o,
		boost::shared_ptr<std::vector<unsigned char> > const& ndat
	) : IOEvent(nproc), fd(o.fd), dat(ndat), start_write(0) { }

public:
	#ifdef USE_POSIX_SELECT
		void add_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			FD_SET(fd, &wr);
		}
		void remove_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			FD_CLR(fd, &wr);
		}
		bool is_in_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			return FD_ISSET(fd, &wr);
		}
		/*Needed to get max FD.*/
		int get_fd(void) const { return fd; }
	#endif

	bool perform_event(Process& host) {
		std::vector<unsigned char>& data = *dat;
		ssize_t rv;
		size_t datsize = data.size() - start_write;
		do {
			errno = 0;
			rv = ::write(fd, (void*) &(data[start_write]), datsize);
		} while(rv < 0 && errno == EINTR);
		if(rv < 0) {
			/*would block, so don't decommit*/
			if(errno == EAGAIN) return 0;
			send_error(host, write_error_message());
			return 1;
		} else {
			if(rv == datsize) {
				/*notify calling process*/
				send_nil(host);
				return 1;
			} else {
				/*incomplete write*/
				start_write += rv;
				return 0;
			}
		}
	}

	friend class PosixIOPort;
};

class ReadEvent : public IOEvent {
private:
	int fd;
	size_t sz;

	ReadEvent(void); // disallowed!
	ReadEvent(
		boost::shared_ptr<ProcessInvoker> const& nproc,
		PosixIOPort const& o,
		size_t nsz
	) : IOEvent(nproc), fd(o.fd), sz(nsz) { }

public:
	#ifdef USE_POSIX_SELECT
		void add_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			FD_SET(fd, &rd);
		}
		void remove_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			FD_CLR(fd, &rd);
		}
		bool is_in_select_event(fd_set& rd, fd_set& wr, fd_set& ex) const {
			return FD_ISSET(fd, &rd);
		}
		/*Needed to get max FD.*/
		int get_fd(void) const { return fd; }
	#endif

	bool perform_event(Process& host) {
		boost::shared_ptr<std::vector<unsigned char> > pndat(
			new std::vector<unsigned char>(sz)
		);
		ssize_t rv;
		void* pdat = (void*)&((*pndat)[0]);
		do {
			rv = ::read(fd, pdat, sz);
		} while(rv < 0 && errno == EINTR);
		if(rv < 0) {
			/*would block, so don't decommit*/
			if(errno == EAGAIN) return 0;
			send_error(host, read_error_message());
		} else if(rv == 0) {
			/*presumably EOF*/
			send_nil(host);
		} else {
			std::vector<unsigned char>& dat = *pndat;
			dat.resize(rv);
			send_data(host, pndat);
		}
	}
};

/*-----------------------------------------------------------------------------
Event creation
-----------------------------------------------------------------------------*/

boost::shared_ptr<Event> PosixIOPort::write(
		boost::shared_ptr<ProcessInvoker> proc,
		boost::shared_ptr<std::vector<unsigned char> >& pdat) {
	if(!writeable) {
		throw IOError(
			std::string("Attempt to write to non-writeable port.")
		);
	}
	/*first check if the relevant FD is immediately writeable*/
	/*On most *nixes, disk filesystems are assumed to be so fast
	that they never block (never mind that in reality they do) and
	thus select()/poll() would immediately reply with "ready".
	Trying to ask the OS to decently poll this just don't work.
	The specs thus make a proviso that '<bc>write-event can
	return with the write actually done, reducing the message-
	passing overhead to the central I/O process.
	*/
	int rv;
	#ifdef USE_POSIX_SELECT
		fd_set rd, wr, exc; FD_ZERO(&rd); FD_ZERO(&wr); FD_ZERO(&exc);
		FD_SET(fd, &wr);
		struct timeval tm; tm.tv_usec = 0; tm.tv_sec = 0;
	#endif
	do {
		errno = 0;
		#ifdef USE_POSIX_SELECT
			rv = ::select(fd + 1, &rd, &wr, &exc, &tm);
		#endif
	} while(rv < 0 && errno == EINTR);
	/*We ignore errors on the select()/poll().  If errors occured,
	just assume that we can't immediately write.
	*/
	/*if select()/poll() succeeded*/
	if(rv == 1
		#ifdef USE_POSIX_SELECT
			&& FD_ISSET(fd, &wr)
		#endif
			) {
		/*can write: try writing*/
		ssize_t wrv;
		size_t len = pdat->size();
		unsigned char* dat = &((*pdat)[0]);
		do {
			errno = 0;
			wrv = ::write(fd, (void*) dat, len);
		} while(wrv < 0 && errno == EINTR);
		/*handle errors except EAGAIN; for EAGAIN, just fall through*/
		if(wrv < 0 && errno != EAGAIN) {
			throw IOError(std::string(write_error_message()));
		}
		if(wrv == len) {
			/*complete write; now we can just exit with NULL*/
			return boost::shared_ptr<Event>();
		}
		if(wrv > 0) {
			/*incomplete write; schedule the rest of the
			data for event.
			*/
			boost::shared_ptr<WriteEvent> rv(
				new WriteEvent(
					proc,
					*this,
					pdat
				)
			);
			/*start at point that was written to*/
			rv->start_write = wrv;
			return boost::static_pointer_cast<Event>(rv);
		}
	}
	/*if we fell through to here, it means we were not able to
	write at all.  So return an event.
	*/
	return boost::shared_ptr<Event>(
		new WriteEvent(
			proc,
			*this,
			pdat
		)
	);
}

/*-----------------------------------------------------------------------------
Core code
-----------------------------------------------------------------------------*/

/*self-pipe trick with SIGCHLD*/
static int sigchld_wr, sigchld_rd;
/*PLANNED (not yet implemented)
When compiled single_threaded, signal handlers also
write to sigchld_wr, in order to (hopefully!)
interrupt pending select()/poll()'s.  We don't depend
on this too much, because we will still raise flags
somewhere - we only depend on it to interrupt our
calls.
When compiled without single_threaded, signals are
blocked on all threads, then at initialization we
start a "hidden" thread whose sole purpose is to
wait for signals.
*/

/*SIGCHLD Known issues:
On Linux < 2.4, each thread's child processes belonged
only to that thread; this meant (1) SIGCHLD gets sent
exactly to the thread which launched that process, and
nobody else, and (2) a thread couldn't waitpid() on any
other thread's child process.  Both behaviors are not
POSIX compliant.  I also can't think of a way to handle
those properly, at least not with the fact that the
events system in `hl` may be launched from any of N
worker threads.

For Linux >= 2.4, a thread could now waitpid() on other
thread's child processes.  However, AFAIK (can't find
references regarding exact behavior) SIGCHLD is still
sent to the launching thread.

For Linux >= 2.6, we have NPTL, which is supposedly
correctly POSIX.

The code here *should* work on Linux 2.4, assuming that
write() from multiple threads simultaneously gets
properly handled (locked!!) by the OS.
*/

static void sigchld_handler(int) {
	ssize_t rv;
	char buf[1]; /*exact data doesn't matter*/
	do {
		errno = 0;
		rv = write(sigchld_wr, buf, 1);
		/*our signal installer actually
		masks out all other signals for
		this handler, so we shouldn't get
		EINTR, but better safe than
		sorry...
		*/
	} while(rv < 0 && errno == EINTR || rv == 0);
	/*All other errors get ignored!*/
	/*Errors:
	EAGAIN
		- we *shouldn't* mark sigchld_wr as nonblocking
	EBADF
		- pretty bad problem, means we failed to init,
		but we *do* check the pipe FD's at init time
		before we even install this sighandler.
	EFAULT
		- nasty, means buf is segfaultable, meaning we
		probably can't return anyway.
	EFBIG
		- not writing to a file, just a pipe.
	EINTR
		- handled by restarting the call.
	EINVAL
		- pretty bad problem, means we failed to init
	EIO
		- not writing to a file, just a pipe.
	ENOSPC
		- that's OK, we only care that there's data
		on the pipe to wake up the select() call
	EPIPE
		- will only happen when we're cleaning up
		and closing everything, so won't matter if
		signal is lost, we're already exiting.
	*/
}

/*empty signal handler, used for e.g. SIGPIPE*/
static void nihilistic_handler(int) { return; }

/*FD_CLOEXEC Known Issues:
Normally FD's will remain open across fork() and exec() calls.
This is useful when piping.  Unfortunately, we rarely even
want to use piping; almost definitely we will pipe only a few
FD's.

Thus, in the general case, when acquiring FD's from the OS,
we must very soon force them to FD_CLOEXEC.  However, this
has problems in a multithreaded implementation which fork()s.
It's possible for one thread to fork()+exec() while another
thread has just, say, called socket().  In this case the
returned socket will *not* have FD_CLOEXEC and will remain
open in the child process, reducing the number of useable
FD's in the process, as well as other weird stuff like tape
drives not rewinding...

This can be solved in many ways:
1.  Ignore CLOEXEC.  After fork() and before exec(), close
everything except for any redirections we want.  This means
about sysconf(_SC_OPEN_MAX) calls to close().  Now suppose
_SC_OPEN_MAX is 65,536...
2.  Like above.  But we just keep closing until we get N
consecutive EBADF failures for close(), where N is some big
number.  Hackish solution which depends on the probability
that FD numbers will skip at most N consecutive values.
3.  Keep track of the largest ever FD we ever see from any
source.  Just loop up to that instead of
sysconf(_SC_OPEN_MAX), which should be faster because the
largest FD will be much smaller than _SC_OPEN_MAX.  However
when multithreaded we have to rely on locks to keep track
of the largest seen FD anyway, so....
4.  Protect all open()+fcntl()/socket()+fcntl/pipe()+fcntl(),
as well as fork(), with a mutex.  While fork() will also
duplicate the mutex in the child, we do an exec() soon
anwyay, so the child's mutex copy gets implicitly deleted.

#4 above seems the best solution but it *might* mean that
some LIBC and other third-party library routines can't be
safely used (since they would internally open some FD's,
which we can't protect with a mutex).
*/
static void force_cloexec(int fd) {
	int flag;
	do {
		errno = 0;
		flag = fcntl(fd, F_GETFD);
	} while(flag < 0 && errno == EINTR);
	if(flag < 0) {
		std::cerr << "cloexec: error during getting of flag"
			<< std::endl;
		exit(1);
	}
	long lflag = ((long) (flag)) | FD_CLOEXEC;
	int rv;
	do {
		errno = 0;
		rv = fcntl(fd, F_SETFD, lflag);
	} while(rv < 0 && errno == EINTR);
	if(flag < 0) {
		std::cerr << "cloexec: error during setting of flag"
			<< std::endl;
		exit(1);
	}
}

void aio_initialize(void) {
	int rv;

	/*---------------------------------------------------------------------
	setup self-pipe
	---------------------------------------------------------------------*/
	int pipefds[2];
	errno = 0;
	rv = pipe(pipefds);
	if(rv < 0) {
		if(errno == ENFILE) {
			std::cerr << "aio_initialize: too many open "
				<< "files during self-pipe "
				<< "initialization, oh noes!"
				<< std::endl;
		} else {
			std::cerr << "aio_initialize: Unexpected error "
				<< "during self-pipe initialization"
				<< std::endl;
		}
		exit(1);
	}
	force_cloexec(pipefds[0]);
	force_cloexec(pipefds[1]);
	/*if the read end is not selectable, fail*/
	if(INVALID_FD_VAL(pipefds[0])) {
		std::cerr << "aio_initialize: self-pipe read FD is "
			<< "not useable for asynchronous I/O."
			<< std::endl;
		exit(1);
	}
	sigchld_rd = pipefds[0]; sigchld_wr = pipefds[1];

	/*---------------------------------------------------------------------
	setup SIGCHLD and SIGPIPE
	---------------------------------------------------------------------*/
	struct sigaction sa;
	/*SIGCHLD*/
	sigfillset(&sa.sa_mask); /*block everything inside handler*/
	sa.sa_flags = 0;
	sa.sa_handler = &sigchld_handler;
	rv = sigaction(SIGCHLD, &sa, 0);
	if(rv < 0) {
		std::cerr << "aio_initialize: Unexpected error during "
			<< "SIGCHLD handler setup."
			<< std::endl;
		exit(1);
	}
	/*SIGPIPE*/
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = &nihilistic_handler;
	rv = sigaction(SIGPIPE, &sa, 0);
	if(rv < 0) {
		std::cerr << "aio_initialize: Unexpected error during "
			<< "SIGPIPE handler setup."
			<< std::endl;
		exit(1);
	}

	/*TODO: initialize EventSet object*/

}

void aio_deinitialize(void) {
	/*TODO: figure out what to clean up, if any*/
}

/*-----------------------------------------------------------------------------
Event Set
-----------------------------------------------------------------------------*/

class EventSetImpl {
public:
	std::set<boost::shared_ptr<IOEvent> > io_events;

	#ifdef USE_POSIX_SELECT
		fd_set rd, wr, exc;
	#endif

	EventSetImpl(void)
		: io_events() {
		#ifdef USE_POSIX_SELECT
			FD_ZERO(&rd);
			FD_ZERO(&wr);
			FD_ZERO(&exc);
		#endif
	}
};

EventSet::EventSet(void) : pimpl(new EventSetImpl) {}
EventSet::~EventSet() {delete pimpl;}

void EventSet::scan_process_invokers(ProcessInvokerScanner* pis) {
	EventSetImpl& event_set = *pimpl;
	typedef std::set<boost::shared_ptr<IOEvent> >::iterator
			io_event_iterator;
	for(io_event_iterator it = event_set.io_events.begin();
			it != event_set.io_events.end();
			++it) {
		(*it)->scan_process_invokers(pis);
	}
}

/*add an event*/
void EventSet::add_event(boost::shared_ptr<Event> evp) {
	EventSetImpl& event_set = *pimpl;
	boost::shared_ptr<IOEvent> iop =
		boost::dynamic_pointer_cast<IOEvent>(evp);
	if(iop) {
		iop->add_select_event(
			event_set.rd,
			event_set.wr,
			event_set.exc
		);
		event_set.io_events.insert(iop);
		return;
	}
	/*if reached here, internal inconsistency*/
	std::cout << "add-event: Internal inconsistency, somehow, got an "
		<< "Event object whose type we are unaware of.  Please "
		<< "contact developers."
		<< std::endl;
	exit(1);
}

/*remove an event*/
void EventSet::remove_event(boost::shared_ptr<Event> evp) {
	EventSetImpl& event_set = *pimpl;
	boost::shared_ptr<IOEvent> iop =
		boost::dynamic_pointer_cast<IOEvent>(evp);
	if(iop) {
		iop->remove_select_event(
			event_set.rd,
			event_set.wr,
			event_set.exc
		);
		event_set.io_events.erase(
			event_set.io_events.find(iop)
		);
		return;
	}
	/*if reached here, internal inconsistency*/
	std::cout << "event-event: Internal inconsistency, somehow, got an "
		<< "Event object whose type we are unaware of.  Please "
		<< "contact developers."
		<< std::endl;
	exit(1);
}


/*non-blocking check for event*/
void EventSet::event_poll(Process& host) {
	EventSetImpl& event_set = *pimpl;
	typedef std::set<boost::shared_ptr<IOEvent> >::iterator
			io_event_iterator;

	/*TODO: check 'sleep and 'system events*/

	int fd_max = 0;
	for(io_event_iterator it = event_set.io_events.begin();
			it != event_set.io_events.end();
			++it) {
		IOEvent& ev = **it;
		#ifdef USE_POSIX_SELECT
			int fd = ev.get_fd();
			if(fd > fd_max) fd_max = fd;
		#endif
	}
	#ifdef USE_POSIX_SELECT
		/*create copies to preserve set-to-monitor*/
		fd_set
			x_rd	= event_set.rd,
			x_wr	= event_set.wr,
			x_exc	= event_set.exc
		;
		struct timeval zero_time = {0, 0};
	#endif
	int rv;
	do {
		errno = 0;
		#ifdef USE_POSIX_SELECT
			rv = select(
				fd_max + 1,
				&x_rd, &x_wr, &x_exc,
				&zero_time
			);
		#endif
	} while(rv < 0 && errno == EINTR);
	if(rv < 0) {
		switch(errno) {
		case EBADF:
			std::cout << "event-poll: Internal inconsistency, "
				<< "somehow, got an invalid FD.  Please "
				<< "contact developers."
				<< std::endl;
			exit(1);
		case EINVAL:
			std::cout << "event-poll: Internal inconsistency, "
				<< "somehow, invalid arguments in system "
				<< "call.  Please contact developers."
				<< std::endl;
			exit(1);
		case ENOMEM:
			std::cout << "event-poll: System reported out-of-memory."
				<< std::endl;
			exit(1);
		default:
			std::cout << "event-poll: unexpected error."
				<< std::endl;
			exit(1);
		}
	} else if(rv == 0) {
		/*nothin' happenin', boss!*/
		return;
	} else {
		/*check each I/O Event*/
		for(io_event_iterator it = event_set.io_events.begin();
				it != event_set.io_events.end();
				) {
			IOEvent& ev = **it;
			io_event_iterator curr = it;
			++it;
			bool check =
				#ifdef USE_POSIX_SELECT
					ev.is_in_select_event(
						x_rd,
						x_wr,
						x_exc
					)
				#endif
			;
			if(check) {
				if(ev.perform_event(host)) {
					#ifdef USE_POSIX_SELECT
						ev.remove_select_event(
							event_set.rd,
							event_set.wr,
							event_set.exc
						);
					#endif
					event_set.io_events.erase(curr);
				}
			}
		}
	}
}

/*the actual event set instance*/
static EventSet actual_event_set;

EventSet& the_event_set(void) { return actual_event_set; }

/*-----------------------------------------------------------------------------
Error messages
-----------------------------------------------------------------------------*/

static char const* write_error_message(void) {
	switch(errno) {
	/*very bad.  internal inconsistency*/
	case EBADF:
	case EFAULT:
		return
			"Internal inconsistency "
			"in write event: EBADF or "
			"EFAULT.  Please contact "
			"developer.";
	case EFBIG:
		return
			"File would grow beyond "
			"OS limits in write event.";
	case EINVAL:
		return
			"Internal inconsistency "
			"in write event/writeable "
			"I/O port: FD is not "
			"valid for writing.  Please "
			"contact developer.";
	case EIO:
		return "Low-level I/O error.";
	case ENOSPC:
		return "Out of space on device.";
	case EPIPE:
		return "Other end of pipe closed.";
	default:
		return "Write event, unknown error...";
	}
}

static char const* read_error_message(void) {
	switch(errno) {
	/*very bad.  internal inconsistency*/
	case EBADF:
	case EFAULT:
		return
			"Internal inconsistency "
			"in read event: EBADF or "
			"EFAULT.  Please contact "
			"developer.";
	case EINVAL:
		return
			"Internal inconsistency "
			"in read event/readable "
			"I/O port: FD is not "
			"valid for reading.  Please "
			"contact developer.";
	case EIO:
		return "Low-level I/O error.";
	case EISDIR:
		return "Can't read from a directory.";
	default:
		return "Read event, unknown error...";
	}
}

