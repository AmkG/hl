#ifndef THREAD_H
#define THREAD_H

#include <pthread>

template <class T>
class Thread {
private:
  pthread_t t;
  static void* entry(void *to_run);
public:
  Thread(T *to_run);
};

template <class T>
void* Thread::entry(void *to_run) {
  T *t = (T*)to_run; 
  (*t)();
}

template <class T>
Thread::Thread(T *to_run) {
  pthread_create(&t, NULL, f, to_run);
  pthread_detach(t);
}

class Mutex {
private:
  pthread_mutex m;
public:
  Mutex() { pthread_mutex_init(&m, NULL); }
  ~Mutex() { pthread_mutex_destroy(&m); }
  void lock() { pthread_mutex_lock(&m); }
  void unlock() { pthread_mutex_unlock(&m); }
};

#endif // THREAD_H
