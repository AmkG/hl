#include "aio.hpp"
#include "aio_thread_backend.hpp"

#include <iostream>

class printIt : public ActionOn {
public:
  AIN *in;
  TaskQueue *q;
  printIt(AIN *in, TaskQueue *q) : in(in), q(q) {}
  void onComplete(const char *data, size_t len, AIOError *e);
};

void printIt::onComplete(const char *data, size_t len, AIOError *e) {
  if (data==NULL)
    std::cout << "An error (or EOF) occurred\n";
  else {
    for (size_t i = 0; i<len; i++)
      std::cout << data[i];
    q->addTask(in->mkTaskRead(new printIt(in, q), 1));
    q->performAll(100);
  }
}

class printOk : public ActionOn {
public:
  void onComplete(const char *data, size_t len, AIOError *e) {
    if (data==NULL)
      std::cout << "error\n";
    else
      std::cout << "ok\n";
  }
};

int main(int argc, char **argv) {
  if (argc!=2) {
    std::cout << "Wrong number of arguments" << std::endl;
    return -1;
  }

  ThreadTaskQueue q;    
  ThreadFileIN in;
  in.open(argv[1]);
  printIt *a = new printIt(&in, &q);
  printIt *a2 = new printIt(&in, &q);
  q.addTask(in.mkTaskRead(a, 1));
  q.addTask(in.mkTaskPeek(a2));
  ThreadFileOUT out;
  out.open("test_out");
  q.addTask(out.mkTaskWrite(new printOk(), "abc", 3));
  q.performAll(100);

  sleep(2); // bad way to wait for them to finish
  
  return 0;
}
