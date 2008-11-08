#include "aio.hpp"
#include "aio_thread_backend.hpp"

#include <iostream>

class printIt : public ActionOn {
public:
  AIN *in;
  void onComplete(const char *data, size_t len, AIOError *e);
};

void printIt::onComplete(const char *data, size_t len, AIOError *e) {
  if (data==NULL)
    std::cout << "An error (or EOF) occurred\n";
  else {
    for (size_t i = 0; i<len; i++)
      std::cout << data[i];
    printIt *a = new printIt();
    a->in = in;
    in->addTaskRead(a, 1);
    in->go(100);
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
    
  ThreadFileIN in;
  in.open(argv[1]);
  printIt *a = new printIt();
  a->in = &in;
  printIt *a2 = new printIt();
  a2->in = &in;
  in.addTaskRead(a, 1);
  in.addTaskPeek(a2);
  ThreadFileOUT out;
  out.open("test_out");
  out.addTaskWrite(new printOk(), "abc", 3);
  in.go(100);
  out.go(100);

  sleep(2); // bad way to wait for them to finish
  
  return 0;
}
