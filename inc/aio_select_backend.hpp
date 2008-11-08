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
  // won't be used
  bool ready(seconds timeout) { return false; }
  void perform();
};

#endif // AIO_SELECT_BACKEND_H
