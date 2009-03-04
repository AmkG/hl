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
*/

/*self-pipe trick with SIGCHLD*/
int sigchld_wr, sigchld_rd;
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

