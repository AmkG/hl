#ifndef WORKERS_H
#define WORKERS_H

#include"lockeds.hpp"

#include<vector>
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
	boost::mutex workqueue_mutex;

public:
	/*initiates the specified number of worker threads
	This function will return only when workers run out
	of work, or if someone signals an exit condition
	*/
	void initiate(void);

	AllWorkers(unsigned int nworkers)
		: exit_condition(0),
		  soft_stop_condition(0),
		  soft_stop_barrier(nworkers),
		  gray_workers(0),
		  total_workers(nworkers),
		  workqueue(),
		  workqueue_mutex()
	{ }

	friend class Worker;
};

class Worker {
	std::vector<Process*> gray_set;
	bool gray_done;
	bool scanning_mode;

	AllWorkers* parent;

	/*worker core*/
	void work(void);

public:
	/*callable, used to launch a thread*/
	void operator()(void);

	explicit Worker(AllWorkers* nparent)
		: parent(nparent),
		gray_set(),
		gray_done(1),
		scanning_mode(0)
	{ }
};

#endif // WORKERS_H

