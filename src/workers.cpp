#include"all_defines.hpp"

#include"types.hpp"
#include"workers.hpp"
#include"processes.hpp"
#include"lockeds.hpp"
#include"symbols.hpp"

#include<boost/noncopyable.hpp>

#include<iostream>


/*-----------------------------------------------------------------------------
AllWorkers
-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
Worker
-----------------------------------------------------------------------------*/

void Worker::operator()(void) {
	try {
		try {
			parent->register_worker(this);
			work();
		} catch(std::exception& e) {
			std::cout << "Unhandled exception:" << std::endl;
			std::cout << e.what() << std::endl;
			std::cout << "aborting a worker thread..."
				<< std::endl;
			return;
		}
	} catch(...) {
		std::cout << "Unknown exception!" << std::endl;
		std::cout << "aborting a worker thread..."
			<< std::endl;
		throw;
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
	bool* soft_stop;
	boost::barrier* soft_stop_barrier;
	#ifndef single_threaded
		bool single_threaded_save;
	#endif
public:
	SoftStop(bool& nsoft_stop, boost::barrier& nsoft_stop_barrier)
		: soft_stop(&nsoft_stop), soft_stop_barrier(&nsoft_stop_barrier) {
		(*soft_stop) = 1;
		soft_stop_barrier->wait();
		#ifndef single_threaded
			single_threaded_save = single_threaded;
			single_threaded = 1;
		#endif
	}
	~SoftStop() {
		#ifndef single_threaded
			single_threaded = single_threaded_save;
		#endif
		(*soft_stop) = 0;
		soft_stop_barrier->wait();
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

void Worker::work(void) {
	ProcessStatus Rstat;
	RunningProcessRef R;
	Process* Q = 0;
	size_t timeslice;

WorkerLoop:
	if(parent->soft_stop_condition) {
		/*first time through, notify requesting thread*/
		parent->soft_stop_barrier.wait();
		/*second time through, wait for requesting thread
		to complete work.
		*/
		parent->soft_stop_barrier.wait();
	}
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

		/*having got the short stick, we now compute
		the trigger point for the next GC
		*/
		T =
		(j >= 512) ? 		1 :
		/*otherwise*/		(512 - j);
		T += 1;
		T *= 2;
	}
	goto WorkerLoop;
}


