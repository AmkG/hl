#ifndef WORKERS_H
#define WORKERS_H

#include"lockeds.hpp"
#include"mutexes.hpp"

#include<vector>
#include<set>
#include<queue>

#include<boost/thread/barrier.hpp>
#include<boost/thread/mutex.hpp>
#include<boost/thread/thread.hpp>
#include<boost/noncopyable.hpp>

class Worker;
class Process;

class AllWorkers : boost::noncopyable {
	bool exit_condition;

	bool soft_stop_condition;
	size_t soft_stop_waiting;
	AppCondVar soft_stop_cv;

	AppMutex general_mtx;

	/*When performing a process collection, this specifies the
	number of workers that haven't consumed their gray set yet.
	In the document doc/process-gc.txt, this is the "N" variable
	*/
	AtomicCounter gray_workers;

	/*set of all workers*/
	size_t total_workers;
	std::vector<Worker*> Ws;

	/*set of known processes*/
	std::vector<Process*> U;
	AppMutex U_mtx;

	std::queue<Process*> workqueue;
	size_t workqueue_waiting;
	AppCondVar workqueue_cv;

	/*default timeslice for processes*/
	size_t default_timeslice;

	/*atomically register a worker into Ws*/
	void register_worker(Worker*);

	/*atomically unregister a worker from Ws*/
	void unregister_worker(Worker*);

	/*pushes a process onto the workqueue, then pops a process*/
	void workqueue_push_and_pop(Process*&);

	/*pops a process from the workqueue.
	blocks if no process is available yet.
	returns 0 if all worker threads have blocked
		(meaning all work has ended)
	returns 1 if a process was successfully popped
	*/
	bool workqueue_pop(Process*&);

	/*checks for a soft-stop condition and blocks while it is true*/
	void soft_stop_check(AppLock&);

public:
	/*initiates the specified number of worker threads
	This function will return only when workers run out
	of work, or if someone signals an exit condition
	*/
	void initiate(size_t);

	/*atomically register a process into U*/
	void register_process(Process*);

	/*request and release soft-stop*/
	void soft_stop_raise(void);
	void soft_stop_lower(void);

	/*pushes a process onto the workqueue*/
	void workqueue_push(Process*);

	AllWorkers();
	~AllWorkers();

	friend class Worker;
};

/*Must be copyable!*/
class Worker {
	std::set<Process*> gray_set;
	bool gray_done;
	bool scanning_mode;

	AllWorkers* parent;

	/*worker core*/
	void work(void);

	/*process marking*/
	void mark_process(Process*);

public:
	/*process-level GC triggering*/
	size_t T;

	/*callable, used to launch a thread*/
	void operator()(void);

	explicit Worker(AllWorkers* nparent)
		: parent(nparent),
		gray_set(),
		gray_done(1),
		scanning_mode(0),
		T(0)
	{ }
};

#endif // WORKERS_H

