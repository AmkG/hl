#include "aio_select_backend.hpp"

#include <sys/select.h>

bool do_poll(int fd, seconds timeout, bool is_read = true) {
  fd_set fds;
  timeval t;
  t.tv_sec = timeout;
  t.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  int ret;
  if (is_read)
    ret = select(fd+1, &fds, NULL, NULL, &t); // select is used just to poll
  else
    ret = select(fd+1, NULL, &fds, NULL, &t)
  if (ret > 0)
    return true;
  else
    return false;
}

bool SelectTaskRead::ready(seconds timeout) {
  do_poll(fd, timeout);
}

void SelectTaskRead::perform() {
  char *buf = new char[to_read+1];
  memset(buf, 0, to_read+1);
  ssize_t res = read(fd, buf, to_read);
  try {
    if (res==0) // eof
      act->onComplete(NULL, 0, NULL);
    else {
      if (res<to_read)
        act->onComplete(buf, res, NULL); // should set the error
      else // everything's good
        act->onComplete(buf, to_read, NULL);
    }
  }
  catch (...) { 
    delete [] buf;
    throw;
  }
  delete [] buf;

  delete this;
}

bool SelectTaskPeek::ready(seconds timeout) {
  do_poll(fd, timeout);
}

void SelectTaskPeek::perform() {
  char c;
  ssize_t res = read(fd, &c, 1);
  lseek(fd, SEEK_CUR, -1); // go back one
  if (res==0) // eof
    act->onComplete(NULL, 0, NULL);
  else // everything's good
    act->onComplete(&c, 1, NULL);
  delete this;
}

bool SelectTaskWrite::ready(seconds timeout) {
  do_poll(fd, timeout, false);
}

void SelectTaskWrite::perform() {
  ssize_t res = write(fd, buf, len);
  if (res<len)
    act->onComplete(NULL, 0, NULL); // shold set the error
  else
    act->onComplete(buf, len, NULL);
  delete this;
}
