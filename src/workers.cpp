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
 * Soft-stop
 */

void barrier(AppLock& l, AppCondVar& cv, size_t& called, size_t& needed) {
	called++;
	if(called == needed) {
		called = 0;
		cv.broadcast();
	} else {
		cv.wait(l);
	}
}
bool AllWorkers::soft_stop_check(void) {
	AppLock l(general_mtx);
	if(!soft_stop_condition) return 1;
	if(exit_condition) return 0;
	/*inform the raiser of the condition that we've stopped*/
	barrier(l, soft_stop_cv, soft_stop_workers, total_workers);
	/*wait for the raiser to complete*/
	barrier(l, soft_stop_cv, soft_stop_workers, total_workers);
	return 1;
}
void AllWorkers::soft_stop_raise(void) {
	AppLock l(general_mtx);
	soft_stop_condition = 1;
	barrier(l, soft_stop_cv, soft_stop_workers, total_workers);
}
void AllWorkers::soft_stop_lower(void) {
	AppLock l(general_mtx);
	soft_stop_condition = 0;
	barrier(l, soft_stop_cv, soft_stop_workers, total_workers);
}

/*
 * Registration
 */

void AllWorkers::register_process(Process* P) {
	AppMutex l(U_mtx);
	U.push_back(P);
}

void AllWorkers::register_worker(Worker* W) {
	AppMutex l(general_mtx);
	Ws.push_back(W);
	total_workers++;
}

void AllWorkers::unregister_worker(Worker* W) {
	AppMutex l(general_mtx);
	size_t l = Ws.size();
	for(size_t i = 0; i < l; ++i) {
		if(Ws[i] == W) {
			Ws[i] = Ws[l - 1];
			Ws.resize(l - 1);
			--total_workers;
			return;
		}
	}
}

/*
 * Workqueue
 */

void AllWorkers::workqueue_push(Process* R) {
	AppLock l(workqueue_mtx);
	bool sig = workqueue.empty();
	workqueue.push(R);
	if(sig) workqueue_cv.broadcast();
}
void AllWorkers::workqueue_push_and_pop(Process*& R) {
	AppLock l(workqueue_mtx);
	/*if workqueue is empty, we'd end up popping what we
	would have pushed anyway, so just short-circuit it
	*/
	if(workqueue.empty()) return;
	workqueue.push(R);
	R = workqueue.front();
	workqueue.pop();
}
bool AllWorkers::workqueue_pop(Process*& R) {
	/*important: order should be workqueue_mtx, then general_mtx*/
	AppLock lw(workqueue_mtx);
	if(!workqueue.empty()) {
		R = workqueue.front();
		workqueue.pop();
		return 1;
	}
	workqueue_waiting++;
	{AppLock lg(general_mtx);
		/*if everyone is waiting, there's no more work!*/
		if(workqueue_waiting == total_workers) {
			exit_condition = 1;
			workqueue_cv.broadcast(lw);
			return 0;
		}
	}
	do {
		workqueue_cv.wait(lw);
	} while(workqueue.empty() && !exit_condition);
	--workqueue_waiting;
	if(exit_condition) return 0;
	R = workqueue.front();
	workqueue.pop();
	return 1;
}

/*-----------------------------------------------------------------------------
Worker
-----------------------------------------------------------------------------*/

void Worker::operator()(void) {
	try {
		try { WorkerRegistration w(this, parent);
			work();
		} catch(std::exception& e) {
			std::err << "Unhandled exception:" << std::endl;
			std::err << e.what() << std::endl;
			std::err << "aborting a worker thread..."
				<< std::endl;
			std::err.flush();
			return;
		}
	} catch(...) {
		std::err << "Unknown exception!" << std::endl;
		std::err << "aborting a worker thread..."
			<< std::endl;
		std::err.flush();
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
	SoftStop(AllWorkers* nparent)
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
				/*Process::unanesthesize should have
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
 * Actual work
 */

/*please refer to file doc/process-gc.txt*/
void Worker::work(void) {
	ProcessStatus Rstat;
	RunningProcessRef R;
	Process* Q = 0;
	size_t timeslice;
	bool alt = 0;

WorkerLoop:
	/*only do the checking on alternate iterations*/
	if(alt) {
		if(!parent->soft_stop_check()) return;
		alt = 0;
	} else	alt = 1;
	if(T) {
		/*trigger GC*/
		if(T == 1) {
			if(R) {
				parent->workqueue_push(R);
				R = 0;
			}
			{SoftStop ss(	parent->soft_stop_condition,
					parent->soft_stop_barrier);
				/*all other threads are now suspended*/
				SymbolScanner ssc(parent->Ws);
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
				} else {
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
	{SoftStop ss(
			parent->soft_stop_condition,
			parent->soft_stop_barrier);
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


