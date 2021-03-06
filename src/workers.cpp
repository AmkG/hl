#include"all_defines.hpp"

#include"types.hpp"
#include"workers.hpp"
#include"processes.hpp"
#include"mutexes.hpp"
#include"lockeds.hpp"
#include"symbols.hpp"
#include"aio.hpp"

#include<boost/noncopyable.hpp>

#include<iostream>


/*-----------------------------------------------------------------------------
AllWorkers
-----------------------------------------------------------------------------*/

/*
 * Unique instance
 */
AllWorkers AllWorkers::workers;

void AllWorkers::set_exit_condition() {
	exit_condition = 1;
}


/*
 * Registration
 */

void AllWorkers::register_process(Process* P) {
	AppLock l(U_mtx);
	U.push_back(P);
}

void AllWorkers::register_worker(Worker* W) {
	{
		AppLock l(general_mtx);
		Ws.push_back(W);
		total_workers++;
		if(soft_stop_condition) {
			soft_stopped_procs.push_back(W);
			goto wait;
		}
		return;
	}
wait:
	W->waiting_sema.wait();
}

void AllWorkers::unregister_worker(Worker* W) {
	AppLock lg(general_mtx);
	size_t l = Ws.size();
	for(size_t i = 0; i < l; ++i) {
		if(Ws[i] == W) {
			Ws[i] = Ws[l - 1];
			Ws.resize(l - 1);
			--total_workers;
			/*check if this has changed any of the
			waiting states
			*/
			if(soft_stop_condition) {
				size_t blocked =
					soft_stopped_procs.size() +
					waitqueue.size() +
					1
				;
				if(blocked >= total_workers) {
					/*ASSUMPTION: we assume that a worker
					never dies while raising the soft-stop
					waiting state.
					*/
					soft_stop_sema.post();
				}
			}
			if(waitqueue.size() > 0) {
				if(waitqueue.size() == total_workers) {
					set_exit_condition();
				}
			}
			return;
		}
	}
}

/*
 * Soft-stop
 */

void AllWorkers::soft_stop_raise(void) {
	size_t num_waiting;
	{ AppLock l(general_mtx);
		soft_stop_condition = 1;
		/*if all other workers are blocked on the
		workqueue, then don't block here either
		*/
		if(total_workers == waitqueue.size() + 1) {
			return;
		}
	}
	/*release the lock, then let the other workers
	notify us.
	*/
	soft_stop_sema.wait();
}
void AllWorkers::soft_stop_lower(void) {
	std::vector<Worker*> stopped;
	{ AppLock l(general_mtx);
		soft_stopped_procs.swap(stopped);
		soft_stop_condition = 0;
	}
	for(size_t i = 0; i < stopped.size(); ++i) {
		stopped[i]->waiting_sema.post();
	}
}
void AllWorkers::soft_stop_check(Worker* W, Process*& R) {
	{ AppLock l(general_mtx);
		if(soft_stop_condition) {
			if(R) {
				workqueue.push(R);
				R = 0;
			}
			soft_stopped_procs.push_back(W);
			size_t blocked =
				soft_stopped_procs.size() +
				waitqueue.size() +
				1
			;
			if(blocked >= total_workers) {
				soft_stop_sema.post();
			}
			goto wait;
		}
		return;
	}
wait:
	W->waiting_sema.wait();
	return;
}

/*
 * Workqueue
 */

/*ISSUE:
We shouldn't be arbitrarily pushing items on the workqueue.
The workqueue doubles as the root set during a process-level
GC; this means that pushes on the workqueue during a
process-level GC should push a black process.
*/
void AllWorkers::workqueue_push(Process* R) {
	AppLock l(general_mtx);
	bool waiting = !waitqueue.empty();
	Process::SetOnlyRunning(R,0);
	workqueue.push(R);
	if(waiting) {
		Worker* W = waitqueue.front(); waitqueue.pop();
		W->waiting_sema.post();
	}
}
void AllWorkers::workqueue_push_and_pop(Process*& R, Worker* W) {
	{ AppLock l(general_mtx);
		/*if workqueue is empty, we'd end up popping what we
		would have pushed anyway, so just short-circuit it
		*/
		if(workqueue.empty()) {
			/*if all other workers are waiting, then this process *is*
			the only one running
			*/
			if(waitqueue.size() == total_workers - 1) {
				 Process::SetOnlyRunning(R,1);
			} else {
				 Process::SetOnlyRunning(R,0);
			}
			return;
		}
		Process::SetOnlyRunning(R,0);
		workqueue.push(R);
		R = workqueue.front();
		workqueue.pop();
		return;
	}
}
bool AllWorkers::workqueue_pop(Process*& R, Worker* W) {
start:
	{ AppLock l(general_mtx);
		if(exit_condition) return 0;
		/*we still have to check soft-stop here, because of
		potential race:
			worker A		worker B
			...			check soft-stop, find none
			enter soft-stop		...mutex block
			get workers not		...mutex block
			  waiting on queue (1)
			raise soft-stop flag	...mutex block
			wait for 1 worker	enter pop
			...sema block		check queue empty (true)
			...sema block		push on waitqueue
			...sema block		wait for work
			...sema block		...sema block
		*/
		if(soft_stop_condition) {
			soft_stopped_procs.push_back(W);
			size_t blocked =
				soft_stopped_procs.size() +
				waitqueue.size() +
				1
			;
			if(blocked >= total_workers) {
				soft_stop_sema.post();
			}
			/*release lock and wait*/
			goto wait;
		}
		if(workqueue.empty()) {
			R = 0;
			if(waitqueue.size() == total_workers - 1) {
				set_exit_condition();
				/*wake up all so that we can all die*/
				while(!waitqueue.empty()) {
					Worker* V = waitqueue.front();
					waitqueue.pop();
					V->waiting_sema.post();
				}
				return 0;
			} else {
				waitqueue.push(W);
				goto wait;
			}
		} else {
			R = workqueue.front(); workqueue.pop();
			Process::SetOnlyRunning(R,0);
			return 1;
		}
	}
wait:
	W->waiting_sema.wait(); goto start;
}
void AllWorkers::workqueue_trypop(Process*& R) {
	AppTryLock l(general_mtx);
	if(!l) {
		R = 0;
		return;
	}
	if(workqueue.empty()) {
		R = 0;
		return;
	} else {
		R = workqueue.front(); workqueue.pop();
		return;
	}
}

/*
 * Initiate
 */

#ifndef single_threaded
	class WorkerThreadCollection : boost::noncopyable {
	private:
		std::vector<Thread<Worker>*> ws;
	public:
		void launch(Worker const& W) {
			ws.push_back(new Thread<Worker>(W));
		}
		~WorkerThreadCollection() {
			for(size_t i = 0; i < ws.size(); ++i) {
				ws[i]->join();
				delete ws[i];
			}
		}
	};
#endif

void AllWorkers::initiate(size_t nworkers, Process* begin, ValueHolderRef& rv) {
	{
		begin->is_main = 1;
		#ifdef DEBUG
			std::cerr << "#workers: " << nworkers << std::endl;
		#endif
		#ifndef single_threaded
			WorkerThreadCollection wtc;
		#endif
		register_process(begin);
		workqueue_push(begin);
		Worker W(this);
		#ifndef single_threaded
			for(size_t i = 1; i < nworkers; ++i) {
				wtc.launch(W);
			}
		#endif
		W(1); // the 1 indicates that it is the "main" thread.
	}
	return_value.swap(rv);
}

/*
 * constructor/destructor
 */
AllWorkers::AllWorkers(void)
	/*set default_timeslice to a much smaller number when testing!*/
	: default_timeslice(1024),
	  soft_stop_condition(0),
	  total_workers(0),
	  return_value() {
}

AllWorkers::~AllWorkers() {
	#ifdef DEBUG
		{AppLock l(general_mtx);
			/*consistency checks*/
			if(Ws.size() != 0) {
				std::cerr
					<< "thread pool destruction:"
					<< std::endl
					<< "Not all workers were unregistered!"
					<< std::endl;
			}
			if(!exit_condition) {
				std::cerr
					<< "thread pool destruction:"
					<< std::endl
					<< "Exit condition not triggered!"
					<< std::endl;
			}
		}
	#endif
	for(size_t i = 0; i < U.size(); ++i) {
		delete U[i];
	}
}

/*
 * Debug
 */

void AllWorkers::report(void) {
	AppLock l(U_mtx);
	std::cerr
		<< "Processes not cleaned: "
		<< U.size()
		<< std::endl;
}

/*-----------------------------------------------------------------------------
Worker
-----------------------------------------------------------------------------*/

class WorkerInitTeardown : boost::noncopyable {
private:
	WorkerInitTeardown(void); //disallowed!
	bool is_main;

public:
	explicit WorkerInitTeardown(bool n_is_main)
		: is_main(n_is_main) {
		if(!is_main) {
			aio_thread_initialize();
		}
	}
	~WorkerInitTeardown() {
		if(!is_main) {
			aio_thread_deinitialize();
		}
	}
};

void Worker::operator()(bool is_main) {
	WorkerInitTeardown wit(is_main);
	if(is_main) {
		// T = 4; // for testing, set to 4: future should be 16384
		T = 16384;
	} else {
		T = 0;
	}
	try {
		try {
			parent->register_worker(this);
			work();
			parent->unregister_worker(this);
		} catch(std::exception& e) {
			std::cerr << "Unhandled exception:" << std::endl;
			std::cerr << e.what() << std::endl;
			std::cerr << "aborting a worker thread..."
				<< std::endl;
			std::cerr.flush();
			parent->unregister_worker(this);
			return;
		}
	} catch(...) {
		std::cerr << "Unknown exception!" << std::endl;
		std::cerr << "aborting a worker thread..."
			<< std::endl;
		std::cerr.flush();
		parent->unregister_worker(this);
		return;
	}
}

/*
 * Helper RAII classes
 */

class MarkingTraverser : public HeapTraverser {
private:
	std::set<Process*>* pgray_set;
public:
	explicit MarkingTraverser(std::set<Process*>& ngray_set)
		: pgray_set(&ngray_set) { }
	virtual void traverse(Generic* gp) {
		HlPid* pp = dynamic_cast<HlPid*>(gp);
		if(pp && pp->process->waiting_and_not_black()){
			pgray_set->insert(pp->process);
		}
	}
};

/*Holds the currently-running process.  If something throws
while we're running that process, just kill the process and
drop its references.
*/
class RunningProcessRef : boost::noncopyable {
public:
	Process* p;
	RunningProcessRef(void) : p(0) { }
	RunningProcessRef& operator=(Process* np) { p = np; }
	~RunningProcessRef() {
		/*DON'T delete p, just kill it*/
		if(p) p->atomic_kill();
	}
	operator Process*&(void) { return p; }
	operator Process* const&(void) const { return p; }

	Process* operator->(void) { return p; }
	Process* operator->(void) const { return p; }
	Process& operator*(void) { return *p; }
	Process& operator*(void) const { return *p; }
};

/*Causes a soft-stop on other worker threads
*/
class SoftStop : boost::noncopyable {
	AllWorkers* parent;
	#ifndef single_threaded
		bool single_threaded_save;
	#endif
public:
	explicit SoftStop(AllWorkers* nparent)
		: parent(nparent) {
		parent->soft_stop_raise();
		#ifndef single_threaded
			single_threaded_save = single_threaded;
			single_threaded = 1;
		#endif
	}
	~SoftStop() {
		#ifndef single_threaded
			single_threaded = single_threaded_save;
		#endif
		parent->soft_stop_lower();
	}
};

class AnesthesizeProcess : boost::noncopyable {
	AllWorkers* parent;
	Process* P;
public:
	bool succeeded;
	AnesthesizeProcess(Process* nP, AllWorkers* nparent)
		: parent(nparent),
		  P(nP),
		  succeeded(nP->anesthesize()) { }
	~AnesthesizeProcess() {
		if(succeeded) {
			if(P->unanesthesize()) {
				/*Process::unanesthesize returns
				true if the process received any
				messages; if so, it should have
				set status to process_running
				*/
				parent->workqueue_push(P);
			}
		}
	}
};

/*
 * Marks all PID's of a process
 */
void Worker::mark_process(Process* P) {
	/*Marking may be invoked in parallel multiple times in
	the same process, if the involved process is dead.
	We should be careful to avoid anything that could
	be unsafe if the process is dead.
	*/
	MarkingTraverser mt(gray_set);
	P->heap().traverse_objects(&mt);
	P->mailbox().traverse(&mt);

	/*does not require atomicity, since only one
	worker thread can perform marking on any
	process at a time.
	*/
	P->blacken();
}

/*
 * Scans symbols for references to processes
 */

class SymbolProcessScanner : public SymbolsTableTraverser {
private:
	std::vector<Worker*>* Wsp;
	size_t i;

	void add(Process* pp) {
		std::set<Process*>& gray_set = (*Wsp)[i]->gray_set;
		gray_set.insert(pp);
		++i;
		if(i >= Wsp->size()) i = 0;
	}

	class SingleSymbolScanner : public HeapTraverser {
	private:
		SymbolProcessScanner* parent;
	public:
		explicit SingleSymbolScanner(SymbolProcessScanner* nparent)
			: parent(nparent) { }
		void traverse(Generic* gp) {
			HlPid* pp = dynamic_cast<HlPid*>(gp);
			if(pp) {
				parent->add(pp->process);
			}
		}
	};

public:
	explicit SymbolProcessScanner(std::vector<Worker*>& Ws)
		: Wsp(&Ws), i(0) { }

	void traverse(Symbol* sp) {
		SingleSymbolScanner sss(this);
		sp->traverse_objects(&sss);
	}
};

/*
 * Scan symbols' notification lists for processes that
 * are about to be deleted.
 */

class SymbolNotificationCleaner : public SymbolsTableTraverser {
private:
	std::set<Process*> dead;

public:
	SymbolNotificationCleaner(std::vector<Process*>& U, size_t j) {
		/*dead processes are from U[j] to U[U.size() - 1]*/
		for(size_t i = j; i < U.size(); ++i) {
			dead.insert(U[i]);
		}
	}
	void traverse(Symbol* sp) {
		sp->clean_notification_list(dead);
	}
};

/*
 * Scan event set for processes waiting on I/O and other OS events
 */
class EventSetScanner : public ProcessInvokerScanner {
private:
	std::vector<Worker*>* Wsp;
	int i;

public:
	explicit EventSetScanner(std::vector<Worker*>& Ws)
		: Wsp(&Ws), i(0) { }

	void traverse(ProcessInvoker const& PI) {
		std::set<Process*>& gray_set = (*Wsp)[i]->gray_set;
		gray_set.insert(PI.P);
		++i;
		if(i >= Wsp->size()) i = 0;
	}
};

/*
 * Actual work
 */

/*please refer to file doc/process-gc.txt*/
void Worker::work(void) {
	ProcessStatus Rstat;
	RunningProcessRef R;
	Process* Q = 0;
	size_t timeslice;

WorkerLoop:
	if(T) {
		/*trigger GC*/
		if(T == 1) {
			if(R) {
				parent->workqueue_push(R);
				R = 0;
			}
			{SoftStop ss(parent);
				/*all other threads are now suspended*/
				/*scan global variables*/
				SymbolProcessScanner ssc(parent->Ws);
				symbols->traverse_symbols(&ssc);
				/*scan aio event set*/
				EventSetScanner ess(parent->Ws);
				the_event_set().scan_process_invokers(&ess);
				/*initialize marking state*/
				for(size_t i = 0; i < parent->total_workers; ++i) {
					parent->gray_workers++; // N
					parent->Ws[i]->gray_done = 0;
					parent->Ws[i]->scanning_mode = 1;
					parent->Ws[i]->in_gc = 1;
				}
			}
			T = 0;
		} else {
			--T;
		}
	}
	parent->soft_stop_check(this, R);
	if(R) {
		parent->workqueue_push_and_pop(R, this);
	} else {
		if(!scanning_mode && !gray_done) {
			parent->workqueue_trypop(R);
			if(!R) goto gray_scan;
		} else if(T > 0) {
			/*just count down to the GC if we can't get any*/
			parent->workqueue_trypop(R);
			if(!R) goto WorkerLoop;
		} else {
			if(!parent->workqueue_pop(R, this)) {
				return; //no more work
			}
		}
	}
	if(scanning_mode) {
		if(R->is_black()) {
			scanning_mode = 0;
			goto gray_scan;
		} else {
			mark_process(R);
		}
	}
	timeslice = parent->default_timeslice;
execute:
	Rstat = R->execute(timeslice, Q);
	/*the multipush hack: when *potentially* multiple processes must
	be pushed onto the workqueue.
	*/
	{boost::scoped_ptr<std::vector<Process*> > to_be_added;
		Process::GrabMultipush(R, to_be_added);
		if(to_be_added) {
			/*make sure that the current process only-running flag goes down*/
			Process::SetOnlyRunning(R, 0); //no possible race ...?
			std::vector<Process*>& to_add = *to_be_added;
			for(size_t i = 0; i < to_add.size(); ++i) {
				Process* P = to_add[i];
				if(in_gc && !P->is_black()) {
					mark_process(P);
				}
				parent->workqueue_push(P);
			}
		}
	}
	switch(Rstat) {
	case process_waiting:
	case process_dead:
		if(R->is_main) {
			ValueHolderRef tmp;
			ValueHolder::copy_object(tmp, R->stack.top());
			parent->return_value.swap(tmp);
		}
		R = 0; // clear
		break;
	case process_change:
		parent->workqueue_push(R);
		R = Q;
		Q = 0;
		if(in_gc && !R->is_black()) {
			mark_process(R);
		}
		Process::SetOnlyRunning(R, 0);//no possible race ...?
		goto execute;
	}
gray_scan:
	if(!scanning_mode && !gray_done) {
		if(!gray_set.empty()) {
			if(R) {
				parent->workqueue_push(R);
				R = 0;
			}
			std::set<Process*>::const_iterator i;
		get_gray:
			i = gray_set.begin();
			Q = *i;
			gray_set.erase(i);
			{AnesthesizeProcess ap(Q, parent);
				if(ap.succeeded) {
					/*NOTE! It's possible for us
					to attempt anesthesizing a
					dead process.  This means that
					mark_process must be safe to
					run on the same process in
					multiple threads provided the
					process is dead.
					*/
					mark_process(Q);
				} else if(!gray_set.empty()) {
					/*if anesthesizing failed, keep
					trying to get one while the
					gray_set isn't empty
					*/
					goto get_gray;
				}
			}
			Q = 0;
		} else {
			gray_done = 1;
			if((--parent->gray_workers) == 0) {
				goto Sweep;
			}
		}
	}
	goto WorkerLoop;

Sweep:
	if(R) {
		parent->workqueue_push(R);
		R = 0;
	}
	{SoftStop ss(parent);
		size_t i, j;
		Process* tmp;
		std::vector<Process*>& U = parent->U;
		size_t l = U.size();
		for(i = 0, j = 0; i < l; ++i) {
			if(U[i]->is_black()) {
				U[i]->whiten();
				/*swap in order to partition
				U into known-live and known-dead
				*/
				tmp = U[i];
				U[i] = U[j];
				U[j] = tmp;
				++j;
			} else {
				U[i]->kill();
			}
		}

		/*first clean the symbol's notification lists*/
		SymbolNotificationCleaner snc(U, j);
		symbols->traverse_symbols(&snc);

		/*now really delete U[j] onwards*/
		for(i = j; i < l; ++i) {
			delete U[i];
		}
		U.resize(j);

		for(i = 0; i < parent->Ws.size(); ++i) {
			parent->Ws[i]->in_gc = 0;
		}

		size_t died = l - j;
		/*having got the short stick, we now compute
		the trigger point for the next GC
		*/
		T =
		(died >= 4096) ? 	1 :
		/*otherwise*/		(4096 - died);
		T += 1;
		T *= 4;
		// for testing, set to 4
		// T = 4;
	}
	goto WorkerLoop;
}


