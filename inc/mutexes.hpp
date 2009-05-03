#ifndef MUTEXES_H
#define MUTEXES_H

/*
This header file contains "App" versions of the os/thread.hpp
objects.

The "App" versions have the following characteristics:
1.	If you -Dsingle_threaded, they are completely empty and
	will be removed by a sufficiently-smart compiler.
	We even lose the dependency on pthreads, so in theory
	we can still work in an environment without pthreads.
2.	If you decide to launch hl in single-threaded mode, these
	will not bother to do locking, or allocate and create
	lock.
*/

#ifndef single_threaded
	#include"thread.hpp"
	#include<boost/scoped_ptr.hpp>
#endif
#include<boost/noncopyable.hpp>

class AppLock;
class AppTryLock;
class AppCondVar;
class AppSemaphore;

class AppMutex : boost::noncopyable {
private:
	#ifndef single_threaded
		Mutex m;
	#endif
	AppMutex(AppMutex const&)
	#ifndef single_threaded
		: m()
	#endif
	{ }
public:
	AppMutex(void)
	#ifndef single_threaded
		: m()
	#endif
	{ }
	
	friend class AppLock;
	friend class AppTryLock;
};

class AppLock : boost::noncopyable {
private:
	#ifndef single_threaded
		Mutex* mp;
	#endif
	AppLock(void); // disallowed!
public:
	AppLock(AppMutex& m)
	#ifndef single_threaded
		: mp(0)
	#endif
	{
		#ifndef single_threaded
			if(!single_threaded) {
				mp = &m.m;
				mp->lock();
			}
		#endif
	}
	~AppLock() {
		#ifndef single_threaded
			if(mp) mp->unlock();
		#endif
	}
	friend class AppCondVar;
};

class AppTryLock : boost::noncopyable {
private:
	#ifndef single_threaded
		Mutex* mp;
		bool success;
	#endif
	AppTryLock(void); //disallowed!
public:
	AppTryLock(AppMutex& m)
	#ifndef single_threaded
		: mp(0), success(0)
	#endif
	{
		#ifndef single_threaded
			if(!single_threaded) {
				mp = &m.m;
				success = mp->trylock();
			} else success = 1;
		#endif
	}
	/*safe bool idiom*/
	inline void unspecified_bool(void) const { }
	typedef void (AppTryLock::*unspecified_bool_type)(void) const;
	operator unspecified_bool_type(void) const {
		#ifndef single_threaded
			return success ? &AppTryLock::unspecified_bool : 0;
		#else
			return &AppTryLock::unspecified_bool;
		#endif
	}
	~AppTryLock() {
		#ifndef single_threaded
			if(success && mp) mp->unlock();
		#endif
	}
	friend class AppCondVar;
};

class AppCondVar : boost::noncopyable {
private:
	#ifndef single_threaded
		CondVar c;
	#endif

public:
	AppCondVar(void)
	#ifndef single_threaded
		: c()
	#endif
	{ }
	void wait(AppLock const& l) {
		#ifndef single_threaded
			if(l.mp) {
				c.wait(*l.mp);
			}
		#endif
	}
	void wait(AppTryLock const& l) {
		#ifndef single_threaded
			if(l && l.mp) {
				c.wait(*l.mp);
			}
		#endif
	}

	void signal(void) {
		#ifndef single_threaded
			if(!single_threaded) c.signal();
		#endif
	}
	void broadcast(void) {
		#ifndef single_threaded
			if(!single_threaded) c.broadcast();
		#endif
	}
};

class AppSemaphore : boost::noncopyable {
private:
	#ifndef single_threaded
		Semaphore s;
	#endif

public:
	AppSemaphore(void)
	#ifndef single_threaded
		: s()
	#endif
	{ }
	void post(void) {
		#ifndef single_threaded
			s.post();
		#endif
	}
	void wait(void) {
		#ifndef single_threaded
			s.wait();
		#endif
	}
	void try_wait(void) {
		#ifndef single_threaded
			return s.try_wait();
		#else
			return 1;
		#endif
	}
};

#endif // MUTEXES_H

