#include "aio.hpp"
#include "aio_thread_backend.hpp"

#include <iostream>

class printIt : public ActionOn {
public:
  void onComplete(const char *data, size_t len, AIOError *e);
};

void printIt::onComplete(const char *data, size_t len, AIOError *e) {
  if (data==NULL)
    std::cout << "An error occurred";
  else
    for (size_t i = 0; i<len; i++)
      std::cout << data[i];
  std::cout << "\n";
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
  in.addTaskRead(new printIt(), 100);
  in.addTaskPeek(new printIt());
  ThreadFileOUT out;
  out.open("test_out");
  out.addTaskWrite(new printOk(), "abc", 3);
  in.go(100);
  out.go(100);

  sleep(1); // bad way to wait for them to finish
  
  return 0;
}
