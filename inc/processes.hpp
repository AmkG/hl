#ifndef PROCESSES_H
#define PROCESSES_H

#include"heaps.hpp"
#include"lockeds.hpp"

#include <vector>
#include <map>

void throw_HlError(char const*);

/*Consider putting this into the process's heap*/
class ProcessStack : public std::vector<Object::ref> { 
public:
  Object::ref & top(size_t off=1){
    if(off > size() || off == 0){
      // consider throwing a VMError instead of an hl-side error
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
    if(off != 0) erase(begin(), begin() + off);
  }

  Object::ref& operator[](size_t pos) {
    if (pos >= size())
      throw_HlError("internal: stack overflow in []");
    return (*static_cast<std::vector<Object::ref>*>(this))[pos];
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

class Process;

/*
 * A mailbox manages a queue of messages sent to a process
 */
class MailBox {
private:
	AppMutex mtx;
	Process & parent;
	ValueHolderRef messages;
public:
	MailBox(Process & parent) : parent(parent) {}
	
	// add a new message to the queue
	void insert(ValueHolderRef & message);

	// extract a message from the queue, copy it to the Process heap
	// return false if queue is empty
	// msg will be a reference to the process local copy
	bool recv(Object::ref & msg);

	// is mailbox empty?
	bool empty();
	
	// removes all messages from the mailbox
	void clear();

	ValueHolderRef& getMessages() {
		return messages;
	}
};

class HlPid;

class Process : public Heap {
private:
	ProcessStatus stat;
	bool black;
	AppMutex mtx;

	MailBox mbox;

	/*in the future, consider using a limited cache*/
	std::map<Symbol*, Object::ref> global_cache;

	AppMutex notification_mtx;
	std::vector<Symbol*> invalid_globals;

	/*invalidates globals which have been changed*/
	void invalidate_changed_globals(void);

        /*extra root objects to scan during GC*/
        std::vector<Object::ref*> extra_roots;

        void push_extra_root(Object::ref & root) { 
          extra_roots.push_back(&root); 
        }

        void pop_extra_root() { extra_roots.pop_back(); }

	/*
	 * When a process suspends its execution (maybe because it consumed its
	 * timeslice) it saves here the next instruction to run when it is 
	 * restarted. If this is 0, then the process has yet to start running
	 * There's no need to wrap this into a shared_array, because a 
	 * reference is already held by the Bytecode in the stack
	 */
	//bytecode_t *next_instruction;

public:
	// spawn a new Process
	// take a continuation (allocated in the current process)
	// and return the HlPid (also allocated in the current process)
	// of the newly created process
	// spawned process si *not* registered or added to a workqueue
	HlPid* spawn(Object::ref cont);

	/*RAII class for extra roots*/
	class ExtraRoot {
	private:
		Process& proc;
		Object::ref val;
	public:
		ExtraRoot(Process& nproc) : proc(nproc), val() {
			proc.push_extra_root(val);
		}
		~ExtraRoot() {
			proc.pop_extra_root();
		}
		inline operator Object::ref const&(void) const {
			return val;
		}
		inline operator Object::ref&(void) {
			return val;
		}
		inline ExtraRoot& operator=(Object::ref const& nval) {
			val = nval;
			return *this;
		}
		inline ExtraRoot& operator=(ExtraRoot const& n) {
			val = n.val;
			return *this;
		}
		inline bool operator==(Object::ref const& o) const {
			return val == o;
		}
	};

	/*holds the bytecode sequence object.
	This is necessary because it's possible
	to lose all references to a bytecode
	sequence while it's being executed,
	in particular, during a k-closure-reuse
	or k-closure-recreate.
	*/
	Object::ref bytecode_slot;

	Process(void)
		: stat(process_running),
		  black(0),
		  mtx(),
		  mbox(*this),
		  global_cache(),
		  notification_mtx(),
		  invalid_globals(),
		  bytecode_slot() { }

/*-----------------------------------------------------------------------------
Global Variable Access
-----------------------------------------------------------------------------*/
	/*notifies the process of changes in a particular global*/
	void notify_global_change(Symbol*);
	/*gets the value of a global*/
	Object::ref global_read(Symbol*);
	/*sets the value of a global*/
	void global_write(Symbol*, Object::ref);
	/*clears the cache of global variables*/
	void global_acquire( void );

/*-----------------------------------------------------------------------------
For process-level garbage collection
-----------------------------------------------------------------------------*/
	/*atomically check if status is process_waiting and it is not marked*/
	bool waiting_and_not_black(void);

	/*adds the message M to this process's mailbox*/
	/*Must atomically insert M to the mailbox, then
	check if this process is waiting.  If so, it should
	set is_waiting to true and change the process state
	to process_running.
	returns true if it was able to insert M, false if
	not (e.g. due to contention on the mailbox)
	*/
	bool receive_message( ValueHolderRef& M, bool& is_waiting);

	/*atomically call MailBox::recv() and set stat to process_waiting
	 if the MailBox is empty*/
	bool extract_message(Object::ref & M);

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
	void blacken(void) {
		black = true;
	}
	/*sets color to white*/
	void whiten(void) {
		black = false;
	}
	/*checks color.  no atomicity necessary*/
	bool is_black(void) {
		return black;
	}

	/*checks if the process is dead.*/
	bool is_dead(void) {
		AppLock l(mtx);
		return stat == process_dead;
	}

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
	MailBox& mailbox(void);

	ProcessStack stack;

	virtual void scan_root_object(GenericTraverser* gt);
};

#endif // PROCESSES_H
