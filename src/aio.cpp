#include"all_defines.hpp"

#include"aio.hpp"
#include"heaps.hpp"
#include"types.hpp"
#include"objects.hpp"
#include"processes.hpp"
#include"workers.hpp"
#include"bytecodes.hpp"

/*Generic code for AIO*/
/*This contains code that is shared across AIO implementations*/

/*internal function for sending a message to a process*/
/*
preconditions:
	stack.top() = message to send
postconditions:
	stack.top() has been popped
*/
static inline void send_message_to(Process* p, ProcessStack& stack) {
	/*prepare message*/
	ValueHolderRef m;
	ValueHolder::copy_object(m, stack.top());
	send_message_to(P, m);
	stack.pop();

	bool is_waiting = 0;
	/*keep sending until we definitely go through any locks or whatnot*/
	while(!p->receive_message(v, is_waiting)) /*do nothing*/;
	if(is_waiting) {
		workers->workqueue_push(p);
	}
}

ProcessInvoker::ProcessInvoker(Process* nP) : P(nP) {
	/*TODO: notify workers of one more process-gc root*/
}
ProcessInvoker::~ProcessInvoker() {
	/*TODO: notify workers of loss of root*/
}

void ProcessInvoker::io_respond(
		Process& host,
		boost::shared_ptr<IOPort> port,
		boost::shared_ptr<std::vector<unsigned char> >& dat) {
	Heap& hp = host; ProcessStack& stack = host.stack;
	/*build objects*/
	HlIOPort* io = hp.create<HlIOPort>();
	io->p = port;
	stack.push(Object::to_ref<Generic*>(io));
	/*any data?*/
	if(!dat || dat->size() == 0) {
		stack.push(Object::nil());
	} else {
		BinObj* e = hp.create<BinObj>();
		e->p.swap(dat);
		stack.push(Object::to_ref<Generi*>(e));
	}
	bytecode_cons(host, stack);

	send_message_to(P, stack);

}

