#ifndef PROCESSES_H
#define PROCESSES_H

#include"heaps.hpp"
#include"types.hpp"
#include <vector>

class ProcessStack : public std::vector<Object::ref> { 
public:
  Object::ref & top(size_t off=1){
    if(off > size() || off == 0){
      throw_HlError("internal: process stack underflow in top()");
    }
    return (*this)[size() - off];
  }

  void pop(size_t num=1){
    if(num > size()){
      throw_HlError( "internal: Process stack underflow in pop()");
    }
    if(num != 0) resize(size() - num);
  }

  void push(Object::ref gp){
    push_back(gp);
  }

  void restack(size_t sz) {
    if(sz > size()){
      throw_HlError( "internal: process stack underflow in restack()");
    }
    size_t off = size() - sz;
    /*Not exactly the best way to do it?*/
    if(sz != 0) erase(begin(), begin() + off);
  }
};

enum ProcessStatus {
	process_dead,
	process_waiting,
	process_anesthesized,
	process_running,

	/*Not actually a valid process status, but
	Process::execute should return this if it
	wants to switch to another process.
	We switch to another process under the following
	conditions:
		1. when we awaken a waiting process by
		   sending it a message.
		2. when we launch a new process.
	*/
	process_change
};

class Process : public Heap {
public:
/*-----------------------------------------------------------------------------
For process-level garbage collection
-----------------------------------------------------------------------------*/
	/*atomically check if status is process_waiting and it is not marked*/
	bool waiting_and_not_black(void) const;

	/*adds the message M to this message*/
	/*Must atomically insert M to the mailbox, then
	check if this process is waiting.  If so, it should
	set is_waiting to true and change the process state
	to process_running.
	returns true if it was able to insert M, false if
	not (e.g. due to contention on the mailbox)
	*/
	bool receive_message(
		boost::scoped_ptr<ValueHolder>& M,
		bool& is_waiting
	);

	/*anesthesizes this process if appropriate*/
	/*Must atomically check if process status is process_waiting,
	and process color is not black.  If so, set to
	process_anesthesized.
	returns true if the process was correctly anesthesized
	*/
	bool anesthesize(void);

	/*unanesthesizes this process if possible*/
	/*Must atomically check if the mailbox is empty,
	and set to process_running if there is a message,
	or set to process_waiting if mailbox is empty.
	returns true if the mailbox was not empty.
	*/
	bool unanesthesize(void);

	/*sets color to black*/
	void blacken(void);
	/*sets color to white*/
	void whiten(void);
	/*checks color.  no atomicity necessary*/
	bool is_black(void);

	/*sets process status to process_dead, and frees its
	heap.
	*/
	void kill(void);
	/*like kill(), but does so atomically (with locks!)*/
	void atomic_kill(void);

	/*Executes the process for a time slice
	caller will protect with a try-catch block, which will
	inform the user of the error and kill this process.
	timeslice is the number of function calls (or whatever
	unit) to process before returning.
	Q is an empty pointer to be filled in if this returns
	process_change.
	returns:
	process_dead - process status was set to dead and its
		memory was freed (i.e. execute() has called 
		atomic_kill()), and caller should silently
		drop this process.
	process_waiting - process status was set to waiting,
		and caller should silently drop this process.
	process_anesthesized - invalid!
	process_running - process status was not changed,
		and caller should return this process to
		the workqueue.
	process_change - process executor will donate its
		remaining timeslice to Q.  Q should be a
		process that is not on the workqueue! (i.e.
		Q should not have been process_running state
		previously).  Used when the process sends a
		message to Q and Q has been waiting, or if
		the process started a new process Q.
		Process::execute is responsible for
		atomically changing Q to process_running.
	*/
	ProcessStatus execute(size_t& timeslice, Process*& Q);

	/*allows access to the heap object*/
	Heap& heap(void);

	/*allows access to the mailbox*/
	LockedValueHolderRef& mailbox(void);

        ProcessStack stack;

	virtual void scan_root_object(GenericTraverser* gt);
};

#endif // PROCESSES_H
