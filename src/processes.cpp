#include"all_defines.hpp"

#include"processes.hpp"
#include"mutexes.hpp"
#include"executors.hpp"

/*
 * Process-level GC functions
 */
/*
Refer to doc/process-gc.txt and in particular src/workers.cpp
*/

bool Process::waiting_and_not_black(void) {
	AppLock l(mtx);
	return (stat == process_waiting) && !black;
}

bool Process::receive_message(ValueHolderRef& M, bool& is_waiting) {
	is_waiting = false;
	AppTryLock l(mtx);
	if(!l) return false; /*failed to lock, retry later*/
	if(stat == process_dead) return true; /*silently succeed*/
	mbox.insert(M);
	if(stat == process_waiting) {
		is_waiting = true;
		stat = process_running;
	}
	return true;
}

bool Process::anesthesize(void) {
	AppLock l(mtx);
	if(stat == process_waiting && !black) {
		stat = process_anesthesized;
		return true;
	} else {
		return false;
	}
}

bool Process::unanesthesize(void) {
	AppLock l(mtx);
	ValueHolderRef tmp;
	mbox.swap(tmp);
	if(tmp.empty()) {
		stat = process_waiting;
		return false;
	} else {
		/*since the sender of the message can't send
		unless it gets the mutex (and we have the
		mutex), we expect mbox to be empty at this
		point
		*/
		/*swap back*/
		mbox.swap(tmp);
		stat = process_running;
		return true;
	}
}

void Process::kill(void) {
	stat = process_dead;
	{ValueHolderRef tmp;
		mbox.swap(tmp);
	}
	global_cache.clear();
	invalid_globals.clear();
	/*TODO: how to free heaps??*/
}
void Process::atomic_kill(void) {
	{AppLock l(mtx);
		stat = process_dead;
		{ValueHolderRef tmp;
			mbox.swap(tmp);
		}
	}
	/*used only when running anyway; since we're dead,
	no need to lock
	*/
	global_cache.clear();
	/*lock notification_mtx in case someone decides to
	notify us at this time
	*/
	{AppLock l(notification_mtx);
		invalid_globals.clear();
	}
	/*TODO: how to free heaps??*/
}

Heap& Process::heap(void) {
	return *this;
}
LockedValueHolderRef& Process::mailbox(void) {
	return mbox;
}

/*
 * Global variables
 */
void Process::invalidate_changed_globals(void) {
	/*create-temp-and-swap*/
	std::vector<Symbol*> temp;
	{AppLock l(notification_mtx);
		temp.swap(invalid_globals);
	}
	typedef std::map<Symbol*, Object::ref> cache_map;
	/*work on temp*/
	for(size_t i = 0; i < temp.size(); ++i) {
		cache_map::iterator it = global_cache.find(temp[i]);
		if(it != global_cache.end()) {
			global_cache.erase(it);
		}
	}
}

void Process::notify_global_change(Symbol* S) {
	AppLock l(notification_mtx);
	invalid_globals.push_back(S);
}

Object::ref Process::global_read(Symbol* S) {
	typedef std::map<Symbol*, Object::ref> cache_map;
	cache_map::iterator it = global_cache.find(S);
	if(it != global_cache.end()) {
		return it->second;
	}
	/*not in cache: read it*/
	ValueHolderRef read;
	S->copy_value_to_and_add_notify(read, this);
	Object::ref rv = read.value();
	other_spaces.insert(read);
	/*now cache*/
	global_cache[S] = rv;
	return rv;
}

/*
 * Heap inheritance
 */

void Process::scan_root_object(GenericTraverser* gt) {
	for(size_t i = 0; i < stack.size(); ++i) {
		gt->traverse(stack[i]);
	}
	/*insert code for traversing cached globals here*/
	/*insert code for traversing process-local vars here*/
}

/*
 * Execution
 */

ProcessStatus Process::execute(size_t& reductions, Process*& Q) {
	/*try*/ {
		::execute(*this, reductions, Q, 0);
	} /*catch(HlError& h) ...*/
	/*In the future, when we catch an HlError,
	get the process's error handler and force it
	onto the stack for future execution. i.e.
	transform a C-side exception into an hl-side
	invocation of (err 'type "message")
	*/
}

