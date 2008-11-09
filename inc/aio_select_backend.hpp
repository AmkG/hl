/*
 * Backend for AIO based on select()
 */

#ifndef AIO_SELECT_BACKEND_H
#define AIO_SELECT_BACKEND_H

#include "aio.hpp"

class SelectTaskRead : public TaskRead {
  friend class SelectFileIN;
private:
  int fd; // work directly with file descriptors
  ActionOn *act;
  size_t to_read;
protected:
  SelectTaskRead(int fd, ActionOn *a, size_t len) 
    : fd(fd), act(a), to_read(len) {}
public:
  ~SelectTaskRead() { delete act; }
  bool ready(seconds timeout);
  void perform();
};

class SelectTaskPeek : public TaskPeek {
  friend class SelectFileIN;
private:
  int fd;
  ActionOn *act;
protected:
  SelectTaskPeek(int fd, ActionOn *a) : fd(fd), act(a) {}
public:
  ~SelectTaskPeek() { delete act; }
  bool ready(seconds timeout);
  void perform();
};

class SelectTaskWrite : public TaskWrite {
  friend class SelectFileOUT;
private:
  int fd;
  ActionOn *act;
  const char *buf;
  size_t len;
protected:
  SelectTaskWrite(int fd, ActionOn *a, const char *buf, size_t len) 
    : fd(fd), act(a), buf(buf), len(len) {}
public:
  ~SelectTaskWrite() { delete act; }
  bool ready(seconds timeout);
  void perform();
};


class SelectFileIN : public FileIN {
private:
  int fd;
public:
  void open(std::string path);
  Task* mkTaskRead(ActionOn *a, size_t how_many) { 
    return new SelectTaskRead(fd, a, how_many);
  }
  Task* mkTaskPeek(ActionOn *a) {
    return new SelectTaskPeek(fd, a);
  }
  void close();
};

class SelectFileOUT : public FileOUT {
private:
  int fd;
public:
  Task* mkTaskWrite(ActionOn *a, char *buf, size_t len) {
    return new SelectTaskWrite(fd, a, buf, len);
  }
  void open(std::string path);
  void close();
};

#endif // AIO_SELECT_BACKEND_H
