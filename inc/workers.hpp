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

class Worker;
class Process;

class AllWorkers {
	bool exit_condition;

	bool soft_stop_condition;
	boost::barrier soft_stop_barrier;

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

	std::queue<Process*> workqueue;
	AppMutex workqueue_mutex;

	/*default timeslice for processes*/
	size_t default_timeslice;

	/*atomically register a worker into Ws*/
	void register_worker(Worker*);

	/*pushes a process onto the workqueue, then pops a process*/
	void workqueue_push_and_pop(Process*&);

	/*pops a process from the workqueue.
	blocks if no process is available yet.
	returns 0 if all worker threads have blocked
		(meaning all work has ended)
	returns 1 if a process was successfully popped
	*/
	bool workqueue_pop(Process*&);

public:
	/*initiates the specified number of worker threads
	This function will return only when workers run out
	of work, or if someone signals an exit condition
	*/
	void initiate(void);

	/*atomically register a process into U*/
	void register_process(Process*);

	/*pushes a process onto the workqueue*/
	void workqueue_push(Process*);

	AllWorkers(unsigned int nworkers)
		: exit_condition(0),
		  soft_stop_condition(0),
		  soft_stop_barrier(nworkers),
		  gray_workers(0),
		  total_workers(nworkers),
		  workqueue(),
		  workqueue_mutex(),
		  default_timeslice(nworkers > 1 ? 256 : 64)
	{ }

	friend class Worker;
};

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

