/*
 * Backend for AIO based on select()
 */

#ifndef AIO_SELECT_BACKEND_H
#define AIO_SELECT_BACKEND_H

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
  friend class SelectFileIN;
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

#endif // AIO_SELECT_BACKEND_H
