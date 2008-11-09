#include "aio.hpp"
#include "aio_select_backend.hpp"

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

class printOne : public ActionOn {
public:
  void onComplete(const char *data, size_t len, AIOError *e) {
    if (data==NULL || len<1)
      std::cout << "error (or EOF) in peek\n";
    else
      std::cout << data[0];
  }
};

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
    
  SelectFileIN in;
  in.open(argv[1]);
  printIt *a = new printIt();
  a->in = &in;
  printOne *a2 = new printOne();
  in.addTaskPeek(a2);
  printOne *a3 = new printOne();
  in.addTaskPeek(a3);
  in.addTaskRead(a, 1);
  SelectFileOUT out;
  out.open("test_out");
  out.addTaskWrite(new printOk(), "abc", 3);
  in.go(100);
  out.go(100);

  return 0;
}
