/*
 * Asynchronous I/O support
 * Abstract classes.
 */

#ifndef AIO_H
#define AIO_H

#include <string>
#include <queue>

// without these my compiler signals an error
#define INTPTR_MIN		(-2147483647-1)
#define INTPTR_MAX		(2147483647)

#include "objects.hpp"

typedef size_t seconds;

class AIOError {
private:
  Object::ref err_fn; // function to be called on error
  std::string err; // error message
public:
  AIOError(Object::ref fn, std::string msg) : err_fn(fn), err(msg) {}
  void invoke(); // invoke the error function with the error message
};

/*
 * An action to perform when an I/O operation is completed
 */
class ActionOn {
public:
  virtual void onComplete(const char *data, size_t len, AIOError *e) = 0;
};

/*
 * A task is an I/O operation to be scheduled
 */
class Task {
public:
  virtual bool ready(seconds timeout) = 0;
  virtual void perform() = 0;
};

/*
 * Specific tasks. The real implementation goes in the backend
 */
class TaskRead : public Task {};
class TaskPeek : public Task {};
class TaskWrite : public Task {};

/*
 * A list of tasks to be performed. The real implementation goes in the backend
 */
class TaskQueue : public std::queue<Task*> {
public:
  // try to perform every action in the given timeout (for each action)
  virtual void performAll(seconds timeout) = 0; 
};

/*
 * Base async. I/O object
 */
class AIO {
protected:
  TaskQueue *tq;
public:
  void addTask(Task *t) { tq->push(t); }
  virtual ~AIO() { delete tq; }
  virtual void close() = 0;
};

class AIN : public AIO {
public:
  virtual void addTaskRead(ActionOn *a, size_t how_many) = 0;
  virtual void addTaskPeek(ActionOn *a) = 0;
};

class AOUT : public AIO {
public:
  virtual void addTaskWrite(ActionOn *a, char *to_write, size_t len) = 0;
};

/*
 * Abstract file I/O.
 */
class FileIN : public AIN {
public:
  virtual void open(std::string path) = 0;
};

class FileOUT : public AOUT {
public:
  virtual void open(std::string path) = 0;
};

#endif // AIO_H
