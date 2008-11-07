/*
 * Backend for AIO based on multiple threads
 */

#ifndef AIO_THREAD_BACKEND_H
#define AIO_THREAD_BACKEND_H

#include "aio.hpp"

#include <fstream>

/*
 * Task classes for thread-based async. I/O
 * every task takes a heap-allocated ActionOn object
 * the task is responsible of deallocation
 */

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
  ~ThreadTaskRead() { delete act; }
  // Tasks based on threads are always ready
  bool ready(seconds timeout) { return true; }
  void perform();
};

class ThreadTaskPeek : public TaskPeek {
  friend class ThreadFileIN;
private:
  std::ifstream & in;
  ActionOn *act;
protected:
  ThreadTaskPeek(std::ifstream & i, ActionOn *a) : in(i), act(a) {}
public:
  ~ThreadTaskPeek() { delete act; }
  bool ready(seconds timeout) { return true; }
  void perform();
};

class ThreadTaskWrite : public TaskWrite {
  friend class ThreadFileOUT;
private:
  std::ofstream & out;
  ActionOn *act;
  char *buf;
  size_t len;
protected:
  ThreadTaskWrite(std::ofstream & o, ActionOn *a, char *b, size_t l) 
    : out(o), act(a), buf(b), len(l) {}
public:
  ~ThreadTaskWrite() { delete act; }
  bool ready(seconds timeout) { return true; }
  void perform();
};

class ThreadTaskQueue : public TaskQueue {
public:
  ~ThreadTaskQueue();
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
  void addTaskPeek(ActionOn *a) {
    addTask(new ThreadTaskPeek(in, a));
  }
  void close() { in.close(); }
};

class ThreadFileOUT : public FileOUT {
private:
  std::ofstream out;
public:
  ThreadFileOUT() { tq = new ThreadTaskQueue; }
  void addTaskWrite(ActionOn *a, char *buf, size_t len) {
    addTask(new ThreadTaskWrite(out, a, buf, len));
  }
  void open(std::string path);
  void close() { out.close(); }
};

#endif // AIO_THREAD_BACKEND_H
