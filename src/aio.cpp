#include "aio.hpp"

void AIOError::invoke() {
  // implementation will depend on the VM runtime
}

void TaskQueue::performAll(seconds timeout) {
  Task *t;
  size_t len = size();
  // each element in the queue will be processed exactly once
  for (size_t i = 0; !empty() && i<len; i++) {
    t = front(); pop();
    if (t->ready(timeout))
      t->perform();
    else
      push(t); // back into the queue
  }
}
