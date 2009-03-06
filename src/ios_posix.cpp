#include"all_defines.hpp"

#include"aio.hpp"

#include<unistd.h>
#include<errno.h>
#include<signal.h>
#include<fcntl.h>

// used for error reporting before aborting
#include<iostream>

/*
POSIX-based asynchronous I/O and signal handling

Note that we don't program to the letter of the POSIX
specs: AFAIK no actual OS conforms perfectly to POSIX.
Preferably we try to be somewhat more resilient and
try to be more defensive when making system calls.
*/

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
	} while(rv < 0 && errno == EINTR);
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

}

void aio_deinitialize(void) {
	/*TODO: figure out what to clean up, if any*/
}

