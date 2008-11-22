#include"all_defines.hpp"

#include"types.hpp"
#include"workers.hpp"
#include"processes.hpp"
#include"mutexes.hpp"
#include"lockeds.hpp"
#include"symbols.hpp"

#include<boost/noncopyable.hpp>

#include<iostream>


/*-----------------------------------------------------------------------------
AllWorkers
-----------------------------------------------------------------------------*/

/*
 * Registration
 */

void AllWorkers::register_process(Process* P) {
	AppLock l(U_mtx);
	U.push_back(P);
}

void AllWorkers::register_worker(Worker* W) {
	AppLock l(general_mtx);
	Ws.push_back(W);
	total_workers++;
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
				if(soft_stop_waiting == total_workers) {
					soft_stop_waiting = 0;
					soft_stop_cv.broadcast();
				}
			}
			if(workqueue_waiting > 0) {
				if(workqueue_waiting == total_workers) {
					workqueue_waiting = 0;
					exit_condition = 1;
					workqueue_cv.broadcast();
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
	AppLock l(general_mtx);
	soft_stop_condition = 1;
	if(workqueue_waiting != 0) {
		workqueue_cv.broadcast();
	}
	soft_stop_waiting++;
	soft_stop_cv.wait(l);
}
void AllWorkers::soft_stop_lower(void) {
	AppLock l(general_mtx);
	soft_stop_condition = 0;
	soft_stop_cv.broadcast();
}

void AllWorkers::soft_stop_check(AppLock& l) {
	while(soft_stop_condition) {
		soft_stop_waiting++;
		if(soft_stop_waiting == total_workers) {
			soft_stop_waiting = 0;
			soft_stop_cv.broadcast();
		} else {
			soft_stop_cv.wait(l);
		}
	}
}

/*
 * Workqueue
 */

void AllWorkers::workqueue_push(Process* R) {
	AppLock l(general_mtx);
	bool sig = workqueue.empty();
	workqueue.push(R);
	if(sig) workqueue_cv.broadcast();
}
void AllWorkers::workqueue_push_and_pop(Process*& R) {
	AppLock l(general_mtx);
	soft_stop_check(l);
	/*if workqueue is empty, we'd end up popping what we
	would have pushed anyway, so just short-circuit it
	*/
	if(workqueue.empty()) return;
	workqueue.push(R);
	R = workqueue.front();
	workqueue.pop();
}
bool AllWorkers::workqueue_pop(Process*& R) {
	AppLock l(general_mtx);
start:
	soft_stop_check(l);
	if(!workqueue.empty()) {
		R = workqueue.front();
		workqueue.pop();
		return 1;
	}
retry:
	workqueue_waiting++;
	if(workqueue_waiting == total_workers) {
		workqueue_waiting = 0;
		exit_condition = 1;
		return 0;
	}
	workqueue_cv.wait(l);
	if(exit_condition) return 0;
	--workqueue_waiting;
	/*if we exited due to a soft-stop, bar*/
	if(soft_stop_condition) goto start;
	if(workqueue.empty()) goto retry;
	R = workqueue.front();
	workqueue.pop();
	return 1;
}

/*
 * Initiate
 */

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

void AllWorkers::initiate(size_t nworkers) {
	#ifndef single_threaded
		WorkerThreadCollection wtc;
	#endif
	Worker W(this);
	#ifndef single_threaded
		for(size_t i = 1; i < nworkers; ++i) {
			wtc.launch(W);
		}
	#endif
	W();
}

/*
 * constructor/destructor
 */

AllWorkers::AllWorkers(void) {
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
	for(size_t i; i < U.size(); ++i) {
		delete U[i];
	}
}

/*-----------------------------------------------------------------------------
Worker
-----------------------------------------------------------------------------*/

void Worker::operator()(void) {
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
	MarkingTraverser mt(gray_set);
	P->heap().traverse_objects(&mt);

	/*scan the mailbox*/
	ValueHolderRef tmp;
	LockedValueHolderRef& mailbox = P->mailbox();
	mailbox.swap(tmp);
	if(!tmp.empty()) {
		/*shouldn't throw: traverse_objects doesn't
		throw, and MarkingTraverser doesn't either
		*/
		tmp->traverse_objects(&mt);
		/*put back the contents into the mailbox*/
		mailbox.swap(tmp);
	}

	/*another process might have sent new messages
	while we were marking - so tmp might not be
	empty.  We just push tmp one-by-one onto the
	mailbox.
	*/
	/*
	We don't have to rescan the new messages - we
	only mark during a process-level GC, and
	messages that are sent while a GC is going on
	cannot contain references to white processes,
	because only black processes are actually
	allowed to run (and by extension, send
	messages).
	*/
	ValueHolderRef tmp2;
	while(!tmp.empty()) {
		tmp.remove(tmp2);	/* --> tmp2 */
		mailbox.insert(tmp2);	/* <-- tmp2*/
	}
	/*would be better to insert them all at once
	rather than one at a time.
	*/

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
		/*TODO: in the future, when we actually implement
		notifications for global variable writes, insert
		code here.
		*/
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
				SymbolProcessScanner ssc(parent->Ws);
				symbols->traverse_symbols(&ssc);
				for(size_t i; i < parent->total_workers; ++i) {
					parent->gray_workers++; // N
					parent->Ws[i]->gray_done = 0;
					parent->Ws[i]->scanning_mode = 1;
				}
			}
			T = 0;
		} else {
			--T;
		}
	}
	/*this also checks for soft-stop for us*/
	if(R) {
		parent->workqueue_push_and_pop(R);
	} else {
		if(!parent->workqueue_pop(R)) {
			return; //no more work
		}
	}
	if(scanning_mode) {
		if(R->is_black()) {
			scanning_mode = 0;
		} else {
			mark_process(R);
		}
	}
	timeslice = parent->default_timeslice;
execute:
	Rstat = R->execute(timeslice, Q);
	switch(Rstat) {
	case process_waiting:
	case process_dead:
		R = 0; // clear
		break;
	case process_change:
		parent->workqueue_push(R);
		R = Q;
		Q = 0;
		if(scanning_mode && !R->is_black()) {
			mark_process(R);
		}
		goto execute;
	}
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

		size_t died = l - j;
		/*having got the short stick, we now compute
		the trigger point for the next GC
		*/
		T =
		(died >= 4096) ? 	1 :
		/*otherwise*/		(4096 - died);
		T += 1;
		T *= 4;
	}
	goto WorkerLoop;
}


