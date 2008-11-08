#include "aio_thread_backend.hpp"

#include <pthread.h>

using namespace std;

// !! no error checking for the moment

void ThreadTaskRead::perform() {
  if (!in) {
    act->onComplete(NULL, 0, NULL); // should create an AIOError
    return;
  }
  
  size_t n = to_read;
  string res;
  for (; n>0; n--) {
    res += in.get();
    if (in.eof()) {
      act->onComplete(NULL, 0, NULL); // should create an AIOError
      return;
    }
  }
  act->onComplete(res.c_str(), res.length(), NULL);
}

void ThreadTaskPeek::perform() {
  if (!in) {
    act->onComplete(NULL, 0, NULL); // should create an AIOError
    return;
  }
  char c = in.peek();
  if (in.eof()) {
    act->onComplete(NULL, 0, NULL); // should create an AIOError
    return;
  }
  act->onComplete(&c, 1, NULL);
}

void ThreadTaskWrite::perform() {
  if (!out) {
    act->onComplete(NULL, 0, NULL); // should create an AIOError
    return;
  }
  out.write(buf, len);
  if (!out) {
    act->onComplete(NULL, 0, NULL); // should create an AIOError
    return;
  }
  act->onComplete(buf, len, NULL);
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

void* do_it(void *data) {
  Task *t = (Task*)data;
  t->perform(); // ?? check for exceptions? It shouldn't throw...
  delete t; // we own the task: it is in the queue no more
  return NULL;
}

void ThreadTaskQueue::add(Task *t) {
  // can't add to the queue while a perform cycle is running
  pthread_mutex_lock(&perform_mutex);
  TaskQueue::add(t);
  pthread_mutex_unlock(&perform_mutex);
}

void ThreadTaskQueue::performAll(seconds timeout) {
  // only one call to performAll per object may be active at any given time
  // no exceptions will be raised within the lock
  pthread_mutex_lock(&perform_mutex);
  // thread-based streams are always ready, there is no need to check
  Task *t;
  while (!empty()) {
    t = front(); pop();
    // start a new thread for each task
    pthread_t thread;
    pthread_create(&thread, NULL, do_it, (void*)t);
    pthread_detach(thread);
  }
  pthread_mutex_unlock(&perform_mutex);
}

void ThreadFileIN::open(string path) {
  in.open(path.c_str());
}

void ThreadFileOUT::open(string path) {
  out.open(path.c_str());
}
