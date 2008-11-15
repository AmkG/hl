#ifndef MUTEXES_H
#define MUTEXES_H

#ifndef single_threaded
	#include<boost/thread/mutex.hpp>
	#include<boost/scoped_ptr.hpp>
#endif
#include<boost/noncopyable.hpp>

class AppLock;
class AppTryLock;

class AppMutex : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<boost::try_mutex> mp;
	#endif
public:
	AppMutex() {
		#ifndef single_threaded
			if(!single_threaded) {
				mp.reset(new boost::try_mutex());
			}
		#endif
	}
	friend class AppLock;
	friend class AppTryLock;
};

class AppLock : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<boost::try_mutex::scoped_lock > lp;
	#endif
	AppLock(); // disallowed!
public:
	AppLock(AppMutex& m) {
		#ifndef single_threaded
			if(!single_threaded) {
				lp.reset(new boost::try_mutex::scoped_lock(*m.mp));
			}
		#endif
	}
};

class AppTryLock : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<boost::try_mutex::scoped_try_lock> lp;
	#endif
	AppTryLock(); //disallowed!
public:
	AppTryLock(AppMutex& m) {
		#ifndef single_threaded
			if(!single_threaded) {
				lp.reset(new boost::try_mutex::scoped_try_lock(*m.mp));
			}
		#endif
	}
	/*returns true if we successfully acquired the lock*/
	operator bool(void) const {
		#ifndef single_threaded
			if(!lp) return 1;
			return lp->locked();
		#else
			return 1;
		#endif
	}
};

#endif // MUTEXES_H

