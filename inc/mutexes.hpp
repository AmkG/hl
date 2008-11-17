#ifndef MUTEXES_H
#define MUTEXES_H

#ifndef single_threaded
	#include"thread.hpp"
	#include<boost/scoped_ptr.hpp>
#endif
#include<boost/noncopyable.hpp>

class AppLock;
class AppTryLock;
class AppCondVar;

class AppMutex : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<Mutex> mp;
	#endif
public:
	AppMutex(void) {
		#ifndef single_threaded
			if(!single_threaded) {
				mp.reset(new Mutex());
			}
		#endif
	}
	friend class AppLock;
	friend class AppTryLock;
};

class AppLock : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<Lock> lp;
	#endif
	AppLock(void); // disallowed!
public:
	AppLock(AppMutex& m) {
		#ifndef single_threaded
			if(!single_threaded) {
				lp.reset(new Lock(*m.mp));
			}
		#endif
	}
	friend class AppCondVar;
};

class AppTryLock : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<TryLock> lp;
	#endif
	AppTryLock(void); //disallowed!
public:
	AppTryLock(AppMutex& m) {
		#ifndef single_threaded
			if(!single_threaded) {
				lp.reset(new TryLock(*m.mp));
			}
		#endif
	}
	/*safe bool idiom*/
	inline void unspecified_bool(void) const { }
	typedef void (AppTryLock::*unspecified_bool_type)(void) const;
	operator unspecified_bool_type(void) const {
		#ifndef single_threaded
			if(!lp) return &AppTryLock::unspecified_bool;
			return (*lp) ? &AppTryLock::unspecified_bool : 0;
		#else
			return &AppTryLock::unspecified_bool;
		#endif
	}
	friend class AppCondVar;
};

class AppCondVar : boost::noncopyable {
private:
	#ifndef single_threaded
		boost::scoped_ptr<CondVar> cp;
	#endif

public:
	AppCondVar(void) {
		#ifndef single_threaded
			if(!single_threaded) {
				cp.reset(new CondVar());
			}
		#endif
	}
	void wait(AppLock const& l) const {
		#ifndef single_threaded
			if(l.lp && cp) {
				cp->wait(*l.lp);
			}
		#endif
	}
	void wait(AppTryLock const& l) const  {
		#ifndef single_threaded
			if(l.lp && cp) {
				cp->wait(*l.lp);
			}
		#endif
	}

	void signal(void) {
		#ifndef single_threaded
			if(cp) {
				cp->signal();
			}
		#endif
	}
	void broadcast(void) {
		#ifndef single_threaded
			if(cp) {
				cp->broadcast();
			}
		#endif
	}
};

#endif // MUTEXES_H

