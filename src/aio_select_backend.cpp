#include "aio_select_backend.hpp"

#include <sys/select.h>

bool SelectTaskRead::ready(seconds timeout) {
  fd_set fds;
  timeval t;
  t.tv_sec = timeout;
  t.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  int ret = select(fd+1, &fds, NULL, NULL, &t); // select is used just to poll
  if (ret > 0)
    return true;
  else
    return false;
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
}
