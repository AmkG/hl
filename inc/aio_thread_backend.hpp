/*
 * Backend for AIO based on multiple threads
 */

#ifndef AIO_THREAD_BACKEND_H
#define AIO_THREAD_BACKEND_H

#include "aio.hpp"

class ThreadTaskRead : public TaskRead {
  friend class ThreadFileIN;
private:
  std::ifstream & in;
  ActionOn *act;
  size_t to_read;
protected:
  ThreadTaskRead(std::ifstream & i, ActionOn *a, size_t len) 
    : in(i), act(a), to_read(len) {}
public:
  // Tasks based on threads are always ready
  bool ready() { return true; }
  void perform();
};

class ThreadTaskQueue : public TaskQueue {
public:
  void performAll(seconds timeout); 
};

class ThreadFileIN : public FileIN {
private:
  std::ifstream in;
public:
  ThreadFileIN() { tq = new ThreadTaskQueue; }
  void open(std::string path);
  void addTaskRead(ActionOn *a, size_t how_many) { 
    addTask(new ThreadTaskRead(in, a, how_many));
  }
  void addTaskPeek(ActionOn *a);
  void close();
};

class ThreadFileOUT : public FileOUT {
private:
  std::ofstream out;
public:
  ThreadFileOUT() { tq = new ThreadTaskQueue; }
  void addTaskWrite(ActionOn *a, char *buf, size_t len);
  void open(std::string path);
  void close();
};

#endif // AIO_THREAD_BACKEND_H
