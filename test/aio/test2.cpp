#include "aio.hpp"
#include "aio_select_backend.hpp"

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
    std::cout << "READ_ERR";
  else {
    for (size_t i = 0; i<len; i++)
      std::cout << data[i];
    q->addTask(in->mkTaskRead(new printIt(in, q), 1));
    q->performAll(100);
  }
}

class printOne : public ActionOn {
public:
  void onComplete(const char *data, size_t len, AIOError *e) {
    if (data==NULL || len<1)
      std::cout << "PEEK_ERR";
    else
      std::cout << data[0];
  }
};

class printOk : public ActionOn {
public:
  void onComplete(const char *data, size_t len, AIOError *e) {
    if (data==NULL)
      std::cout << "WRITE_ERR";
    else
      std::cout << "WRITE_OK";
  }
};

int main(int argc, char **argv) {
  if (argc!=2) {
    std::cout << "Wrong number of arguments" << std::endl;
    return -1;
  }
    
  SelectFileIN in;
  TaskQueue q;
  in.open(argv[1]);
  q.addTask(in.mkTaskPeek(new printOne()));
  q.addTask(in.mkTaskPeek(new printOne()));
  q.addTask(in.mkTaskRead(new printIt(&in, &q), 1));
  SelectFileOUT out;
  out.open("test_out");
  q.addTask(out.mkTaskWrite(new printOk(), "abc", 3));
  q.performAll(100);

  return 0;
}
