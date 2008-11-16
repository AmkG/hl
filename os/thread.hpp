#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

#include <boost/noncopyable.hpp>

template <class T>
class Thread : boost::noncopyable {
private:
  pthread_t t;
public:
  Thread(T const& to_run);
};

template <class T>
static void* _os_thread_bounce_fun(T *t) {
  /*
   * consider using a smart pointer instead of
   * this dorky try-catch.
   */
  try {
    t();
    delete t;
  } catch (...) {
    delete t;
    throw;
  }
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
}

class Mutex : boost::noncopyable {
private:
  pthread_mutex_t m;
  void lock() { pthread_mutex_lock(&m); }
  void unlock() { pthread_mutex_unlock(&m); }
public:
  Mutex() { pthread_mutex_init(&m, NULL); }
  ~Mutex() { pthread_mutex_destroy(&m); }

  friend class Lock;
};

class Lock : boost::noncopyable {
private:
  Mutex* m;
public:
  explicit Lock(Mutex& nm) : m(&nm) { nm.lock(); }
  ~Lock() { m->unlock(); }
};

#endif // THREAD_H
