#include "aio_thread_backend.hpp"

#include <boost/thread.hpp>

using namespace std;

// !! no error checking for the moment

void ThreadTaskRead::perform() {
  size_t n = to_read;
  string res;
  for (; n>0; n--) 
    res += in.get();
  act->onComplete(res.c_str(), res.length(), NULL);
}

void ThreadTaskPeek::perform() {
  char c = in.peek();
  act->onComplete(&c, 1, NULL);
}

void ThreadTaskWrite::perform() {
  out.write(buf, len);
  act->onComplete(buf, len, NULL);
}

class doIt {
public:
  Task *t;
  doIt(Task *atask) : t(atask) {}
  void operator()() { t->perform(); }
};

void ThreadTaskQueue::performAll(seconds timeout) {
  // thread-based streams are always ready, there is no need to check
  Task *t;
  while (!empty()) {
    t = front(); pop();
    // start a new thread for each task
    boost::thread to_run(doIt(t));
  }
}

void ThreadFileIN::open(string path) {
  in.open(path.c_str());
}

void ThreadFileOUT::open(string path) {
  out.open(path.c_str());
}
