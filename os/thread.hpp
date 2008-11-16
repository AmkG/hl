#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

template <class T>
class Thread : boost::noncopyable {
private:
  pthread_t t;
public:
  Thread(T const& to_run);
};

template <class T>
static void* _os_thread_bounce_fun(void *v) {
  boost::scoped_ptr<T> t((T*) v);
  (*t)();
  return NULL;
}

template <class T>
Thread<T>::Thread(T const& to_run) {
  /*
   * to_run is just used as a template; the new thread is
   * given a copy, which that thread is responsible for
   * deleting.
   * Necessary since we just detach the thread and therefore
   * cannot easily determine when or if the thread is
   * already dead, and therefore if we should or shouldn't
   * delete to_run.
   */
  pthread_create(&t, NULL, _os_thread_bounce_fun<T>, new T(to_run));
  pthread_detach(t); // consider allowing to join
  /*alternatively: consider joining on destruction of Thread<T>*/
}

class Mutex : boost::noncopyable {
private:
  pthread_mutex_t m;
  // prevent user from using anything other than a Lock object
  void lock() { pthread_mutex_lock(&m); } // consider throwing if an error occurs
  bool trylock() { return pthread_mutex_trylock(&m) == 0; }
  void unlock() { pthread_mutex_unlock(&m); }
public:
  Mutex() { pthread_mutex_init(&m, NULL); }
  ~Mutex() { pthread_mutex_destroy(&m); }

  friend class Lock;
  friend class TryLock;
  friend class CondVar;
};

class Lock : boost::noncopyable {
private:
  Mutex* m;
public:
  explicit Lock(Mutex& nm) : m(&nm) { nm.lock(); }
  ~Lock() { m->unlock(); }

  friend class CondVar;
};

class TryLock : boost::noncopyable {
private:
  Mutex* m;
public:
  explicit TryLock(Mutex& nm) {
    m = (nm.trylock()) ? &nm : 0 ;
  }
  ~TryLock() { if(m) m->unlock(); }

  friend class CondVar;

  /*safe bool idiom*/
  typedef Mutex* (TryLock::*unspecified_bool_type);
  operator unspecified_bool_type() const {
    return m ? &TryLock::m : 0 ;
  }
};

class CondVar : boost::noncopyable {
private:
  pthread_cond_t cv;
public:
  CondVar() { pthread_cond_init(&cv, NULL); }
  ~CondVar() { pthread_cond_destroy(&cv); }
  // prevent user from not locking the mutex - get a
  // Lock object instead of a Mutex.
  void wait(Lock const& l) { pthread_cond_wait(&cv, &(l.m->m)); }
  // consider throwing on error

  void wait(TryLock const& l) { pthread_cond_wait(&cv, &(l.m->m)); }
  // should throw an error if trylock hasn't acquired

  // only error is an uninitialized condvar; since
  // constructing a valid object requires calling the
  // ctor, that is unlikely to happen.
  void signal(void) { pthread_cond_signal(&cv); }
  void broadcast(void) { pthread_cond_broadcast(&cv); }
};

#endif // THREAD_H
