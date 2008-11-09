#include "aio_thread_backend.hpp"

#include <pthread.h>

using namespace std;

// !! no error checking for the moment

void start_thread(void*(f)(void*), void *data) {
  pthread_t thread;
  pthread_create(&thread, NULL, f, data);
  pthread_detach(thread);
}

void* ThreadTaskRead::do_read(void *data) {
  ThreadTaskRead *t = (ThreadTaskRead*)data;
  
  if (!t->in) {
    t->act->onComplete(NULL, 0, NULL); // should create an AIOError
    delete t; 
    return NULL;
  }
  
  size_t n = t->to_read;
  string res;
  for (; n>0; n--) {
    res += t->in.get();
    if (t->in.eof()) {
      t->act->onComplete(NULL, 0, NULL); // should create an AIOError
      delete t;
      return NULL;
    }
  }
  t->act->onComplete(res.c_str(), res.length(), NULL);  

  delete t; // we own the task: it is in the queue no more
  return NULL;
}

void ThreadTaskRead::perform() {
  start_thread(ThreadTaskRead::do_read, (void*)this);
}

void* ThreadTaskPeek::do_peek(void *data) {
  ThreadTaskPeek *t = (ThreadTaskPeek*)data;
  if (!t->in) {
    t->act->onComplete(NULL, 0, NULL); // should create an AIOError
    delete t;
    return NULL;
  }
  char c = t->in.peek();
  if (t->in.eof()) {
    t->act->onComplete(NULL, 0, NULL); // should create an AIOError
    delete t;
    return NULL;
  }
  t->act->onComplete(&c, 1, NULL);  

  delete t;
  return NULL;
}

void ThreadTaskPeek::perform() {
  start_thread(ThreadTaskPeek::do_peek, (void*)this);
}

void* ThreadTaskWrite::do_write(void *data) {
  ThreadTaskWrite *t = (ThreadTaskWrite*)data;

  if (!t->out) {
    t->act->onComplete(NULL, 0, NULL); // should create an AIOError
    delete t;
    return NULL;
  }
  t->out.write(t->buf, t->len);
  if (!t->out) {
    t->act->onComplete(NULL, 0, NULL); // should create an AIOError
    delete t;
    return NULL;
  }
  t->act->onComplete(t->buf, t->len, NULL);  

  delete t;
  return NULL;
}

void ThreadTaskWrite::perform() {
  start_thread(ThreadTaskWrite::do_write, (void*)this);
}

ThreadTaskQueue::ThreadTaskQueue() {
  pthread_mutex_init(&perform_mutex, NULL);
}

ThreadTaskQueue::~ThreadTaskQueue() {
  pthread_mutex_destroy(&perform_mutex);
  // delete all the remaining tasks
  Task *t;
  while (!empty()) {
    t = front(); pop();
    // should tasks be forced to execute before deletion?
    delete t;
  }
}

void ThreadTaskQueue::addTask(Task *t) {
  // can't add to the queue while a perform cycle is running
  pthread_mutex_lock(&perform_mutex);
  TaskQueue::addTask(t);
  pthread_mutex_unlock(&perform_mutex);
}

void ThreadTaskQueue::performAll(seconds timeout) {
  // only one call to performAll per object may be active at any given time
  // no exceptions will be raised within the lock
  pthread_mutex_lock(&perform_mutex);
  TaskQueue::performAll(timeout);
  pthread_mutex_unlock(&perform_mutex);
}

void ThreadFileIN::open(string path) {
  in.open(path.c_str());
}

void ThreadFileOUT::open(string path) {
  out.open(path.c_str());
}
