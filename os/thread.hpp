#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

template <class T>
class Thread : boost::noncopyable {
private:
  pthread_t t;
public:
  void join();
  Thread(T const& to_run);
  ~Thread();
};

template <class T>
static void* _os_thread_bounce_fun(void *v) {
  boost::scoped_ptr<T> t((T*) v);
  (*t)();
  return NULL;
}

template <class T>
void Thread<T>::join() {
  pthread_join(t, NULL);
}

template <class T>
Thread<T>::Thread(T const& to_run) {
  /*
   * to_run is just used as a template; the new thread is
   * given a copy, which that thread is responsible for
   * deleting.
   * The created thread will automatically delete this
   * itself.
   */
  pthread_create(&t, NULL, _os_thread_bounce_fun<T>, new T(to_run));
}

template <class T>
Thread<T>::~Thread() {
  pthread_detach(t);
}

class Mutex : boost::noncopyable {
private:
  pthread_mutex_t m;
public:
  Mutex() { pthread_mutex_init(&m, NULL); }
  ~Mutex() { pthread_mutex_destroy(&m); }
  void lock() { pthread_mutex_lock(&m); }
  bool trylock() { return pthread_mutex_trylock(&m) == 0; }
  void unlock() { pthread_mutex_unlock(&m); }

  friend class CondVar;
};

class CondVar : boost::noncopyable {
private:
  pthread_cond_t cv;
public:
  CondVar() { pthread_cond_init(&cv, NULL); }
  ~CondVar() { pthread_cond_destroy(&cv); }
  void wait(Mutex& m) { pthread_cond_wait(&cv, &m.m); }


  // only error is an uninitialized condvar; since
  // constructing a valid object requires calling the
  // ctor, that is unlikely to happen.
  void signal(void) { pthread_cond_signal(&cv); }
  void broadcast(void) { pthread_cond_broadcast(&cv); }
};

class Semaphore : boost::noncopyable {
private:
  sem_t sm;
public:
  Semaphore(unsigned int val = 0) { sem_init(&sm, 0, val);  }
  ~Semaphore() { sem_destroy(&sm); }
  void post(void) { sem_post(&sm); }
  void wait(void) {
    int rv;
    do {
      errno = 0;
      rv = sem_wait(&sm);
    } while(rv < 0 && errno == EINTR);
  }
  bool try_wait(void) {
    int rv;
    do {
      errno = 0;
      rv = sem_trywait(&sm);
    } while(rv < 0 && errno == EINTR);
    if(rv < 0 && errno == EAGAIN) return 0;
    else return 1;
  }
};

#endif // THREAD_H
